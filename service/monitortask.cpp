// 【阅读导引】
// 单路采集 Worker：按源节奏读取视频，向独立检测线程派发样本，发送帧副本和状态。
// 由 MainWidget 移入采集线程；普通直接调用不会因为 moveToThread 自动切换线程。

// 实现视频源打开、采集节拍控制、文件循环播放和跨线程帧信号发送。
#include "monitortask.h"
#include "detectionworker.h"
#include <QThread>

#include <QtGlobal>
#include <QFileInfo>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <cmath>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
MonitorTask::MonitorTask(QObject *parent)
    : QObject(parent),
      capture_timer_(new QTimer(this)),
      running_(false), detection_enabled_(false), detection_confidence_(0.5),
      event_active_(false), last_event_ms_(0)
{
    // 精确定时器按视频帧率触发读取；QTimer 会随父对象一起移动到采集线程。
    capture_timer_->setTimerType(Qt::PreciseTimer);
    capture_timer_->setSingleShot(true);
    detection_thread_ = new QThread(this);
    detection_worker_ = new DetectionWorker;
    detection_worker_->moveToThread(detection_thread_);
    connect(detection_thread_, &QThread::finished, detection_worker_, &QObject::deleteLater);
    detection_thread_->start();
    connect(capture_timer_, &QTimer::timeout,
            this, &MonitorTask::captureNextFrame);
}

// 对象销毁时释放本类负责的资源；依赖的外部对象需按约定保持有效。
MonitorTask::~MonitorTask()
{
    stopCapture();
    // 先等推理返回，确保 worker 不会在采集对象销毁后投递结果。
    detection_thread_->quit();
    detection_thread_->wait();
}

// 返回已打开后端报告的解码线程数，仅在该 Worker 所属线程读取。
int MonitorTask::decoderThreadCount() const
{
    return capture_.isOpened() ? qRound(capture_.get(cv::CAP_PROP_N_THREADS)) : 0;
}

// 兼容旧配置调用；文件由源 FPS 调度，摄像头由设备驱动，不再使用统一延时。
void MonitorTask::configureCaptureInterval(int milliseconds)
{
    Q_UNUSED(milliseconds);
}

// 更新检测开关、路径和阈值并加载模型；失败关闭检测但不停止普通采集。
void MonitorTask::configureDetection(bool enabled, const QString &modelPath,
                                     double confidence)
{
    detection_enabled_ = enabled && config_.channelId >= 1 && config_.channelId <= 4;
    detection_model_path_ = modelPath.trimmed();
    detection_confidence_ = qBound(0.05, confidence, 0.99);
    const quint64 generation = ++detection_generation_;
    detection_ready_ = false;
    detection_boxes_.clear();
    event_active_ = false;
    detection_clock_.invalidate();
    if (detection_enabled_ && !QFileInfo::exists(detection_model_path_)) {
        detection_enabled_ = false;
        emit detectionMessage(config_.channelId,
            QStringLiteral("YOLO 模型文件不存在：%1").arg(detection_model_path_));
    }
    auto worker = detection_worker_;
    enabled = detection_enabled_;
    // File detection is handled once on the source timeline by SourceAnalysisWorker.
    enabled = enabled && config_.sourceType == ChannelSourceType::Camera;
    const QString path = detection_model_path_;
    const double threshold = detection_confidence_;
    QMetaObject::invokeMethod(worker, [this, worker, generation, enabled, path, threshold] {
        const QString error = worker->configure(enabled, path, threshold);
        const QString backend=worker->backend();
        QMetaObject::invokeMethod(this, [this, generation, enabled, error, backend] {
            if (generation != detection_generation_) return;
            detection_ready_ = enabled && error.isEmpty();
            detection_backend_=backend;
            if (!error.isEmpty()) {
                detection_enabled_ = false;
                emit detectionMessage(config_.channelId, QStringLiteral("YOLO 模型加载失败：%1").arg(error));
            } else if (enabled) emit detectionMessage(config_.channelId, QStringLiteral("智能检测已启用：")+backend);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

// 先释放旧输入，再打开摄像头或文件、确定帧率、启动本线程定时器。
void MonitorTask::startCapture(const ChannelConfig &config)
{
    // 重启前先释放旧数据源，防止摄像头或文件句柄泄漏。
    stopCapture();
    config_ = config;

    bool opened = false;
    if (config_.sourceType == ChannelSourceType::Camera) {
        opened = config_.cameraIndex >= 0
                && capture_.open(config_.cameraIndex, cv::CAP_ANY);
    } else if (!config_.filePath.trimmed().isEmpty()) {
        opened = openFileCapture();
    }

    if (!opened) {
        emit errorOccurred(
            config_.channelId,
            config_.sourceType == ChannelSourceType::Camera
                ? QStringLiteral("无法打开摄像头 %1，设备可能断开或被占用")
                      .arg(config_.cameraIndex)
                : QStringLiteral("无法打开视频文件：%1").arg(config_.filePath)
        );
        return;
    }

    // 部分摄像头或视频文件不会报告可靠帧率，此时使用 25 FPS 兜底。
    double fps = capture_.get(cv::CAP_PROP_FPS);
    if (!std::isfinite(fps) || fps < 1.0 || fps > 240.0) {
        fps = 25.0;
    }

    source_fps_ = fps;
    next_frame_index_ = 0;
    playback_clock_.restart();
    running_ = true;
    capture_timer_->start(0);
    emit sourceReady(config_.channelId, source_fps_);
    emit captureStateChanged(config_.channelId, true);
}

// 停止定时器并释放 VideoCapture；仅在之前运行时发送状态变化。
void MonitorTask::stopCapture()
{
    ++source_generation_;
    ++detection_generation_;
    cached_box_position_ms_=-1;
    detection_ready_ = false;
    detection_boxes_.clear();
    if (capture_timer_->isActive()) {
        capture_timer_->stop();
    }
    if (capture_.isOpened()) {
        capture_.release();
    }

    if (running_) {
        running_ = false;
        emit captureStateChanged(config_.channelId, false);
    }
}

// 定时读取画面，派发有界异步推理并叠加最近结果；每轮采集都独立发布画面。
void MonitorTask::captureNextFrame()
{
    if (!running_ || !capture_.isOpened()) {
        return;
    }

    cv::Mat frame;
    if (!readNextFrame(frame)) {
        emit errorOccurred(config_.channelId,
                           QStringLiteral("读取视频帧失败，当前通道已停止"));
        stopCapture();
        return;
    }

    // 每路最多一个在途推理；忙时继续预览，不排队旧帧。
    if (detection_enabled_ && detection_ready_ && !detection_pending_
            && (!detection_clock_.isValid() || detection_clock_.elapsed() >= 500)) {
        detection_clock_.restart();
        detection_pending_ = true;
        const quint64 generation = detection_generation_;
        const cv::Mat sample = frame.clone();
        auto worker = detection_worker_;
        QMetaObject::invokeMethod(worker, [this, worker, generation, sample] {
            QElapsedTimer timer; timer.start();
            const DetectionResult result = worker->detect(sample);
            const qint64 elapsed = timer.elapsed();
            QMetaObject::invokeMethod(this, [this, generation, result, elapsed] {
                detection_pending_ = false;
                // 换源、停用后的旧结果不能触发新通道事件。
                if (generation != detection_generation_ || !running_ || !detection_enabled_) return;
                emit detectionCycleCompleted(config_.channelId, elapsed);
                detection_boxes_ = result.boxes;
                if (!result.error.isEmpty()) {
                    detection_enabled_ = false;
                    detection_boxes_.clear();
                    emit detectionMessage(config_.channelId, QStringLiteral("智能检测运行失败，已关闭：%1").arg(result.error));
                    return;
                }
                if(result.backend!=detection_backend_){
                    detection_backend_=result.backend;
                    emit detectionMessage(config_.channelId,"检测后端："+result.backend);
                }
                if (result.found) {
                    last_person_seen_.restart();
                    const qint64 now = QDateTime::currentMSecsSinceEpoch();
                    if (!event_active_ && now - last_event_ms_ >= 10000) {
                        event_active_ = true; last_event_ms_ = now;
                        emit personDetected(config_.channelId,
                            QStringLiteral("person-%1-%2").arg(config_.channelId).arg(now),
                            QDateTime::currentDateTime(), result.confidence);
                    }
                } else if (event_active_ && last_person_seen_.isValid() && last_person_seen_.elapsed() > 3000) {
                    event_active_ = false;
                }
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }
    if(config_.sourceType==ChannelSourceType::File){
        const double ms=capture_.get(cv::CAP_PROP_POS_MSEC);
        source_position_ms_=std::isfinite(ms)&&ms>=0?qRound64(ms):0;
        if(cached_box_position_ms_<0||qAbs(source_position_ms_-cached_box_position_ms_)>500)detection_boxes_.clear();
        if(detection_enabled_)emit sourcePosition(config_.channelId,source_position_ms_,source_generation_);
    }
    for (const auto &box : detection_boxes_) {
        const cv::Rect clipped = box & cv::Rect(0, 0, frame.cols, frame.rows);
        if (!clipped.empty()) cv::rectangle(frame, clipped, cv::Scalar(0,255,0), 2);
    }
    publishFrame(frame);
    // 单次定时器让槽返回事件循环，停止/换源请求有机会执行。
    // 本地文件使用绝对期限，扣除本轮解码与处理耗时；摄像头立即请求下一帧。
    const int delay = config_.sourceType == ChannelSourceType::Camera ? 0
        : qMax(0, static_cast<int>(std::ceil(next_frame_index_ * 1000.0 / source_fps_
                                          - playback_clock_.elapsed())));
    if (running_) capture_timer_->start(delay);
}

// 集中设置文件解码参数与后端回退，避免每个文件源无上限扩张解码线程。
bool MonitorTask::openFileCapture()
{
    // FFmpeg 的解码线程数只能在打开时设置；首次打开和 EOF 重开共用此上限。
    // 四路文件源不再各自按 CPU 核数建立解码线程与帧缓存。
    return capture_.open(config_.filePath.toStdString(), cv::CAP_FFMPEG,
                         std::vector<int>{cv::CAP_PROP_N_THREADS, 2});
}

// 摄像头直接读取；文件按单调时钟定位当前时间，落后时跳过过期帧，EOF 循环。
bool MonitorTask::readNextFrame(cv::Mat &frame)
{
    if (config_.sourceType == ChannelSourceType::Camera)
        return capture_.read(frame) && !frame.empty();

    const qint64 target = qMax(next_frame_index_ + 1,
        static_cast<qint64>(std::floor(playback_clock_.elapsed() * source_fps_ / 1000.0)) + 1);
    qint64 framesToAdvance = target - next_frame_index_;
    next_frame_index_ = target;
    const int frameCount = static_cast<int>(capture_.get(cv::CAP_PROP_FRAME_COUNT));
    if (frameCount > 0) framesToAdvance = (framesToAdvance - 1) % frameCount + 1;
    for (qint64 i = 0; i < framesToAdvance; ++i) {
        if (capture_.grab()) continue;
        // 文件结束后回到第一帧，实现持续循环预览。
        capture_.set(cv::CAP_PROP_POS_FRAMES, 0);
        if (!capture_.grab()) {
            // 部分短文件后端无法从 EOF seek 回首帧，重新打开该文件后重试一次。
            // 仅处理文件源，不重启摄像头，也不无限循环掩盖损坏文件。
            if (!openFileCapture()
                    || !capture_.grab()) return false;
        }
    }
    return capture_.retrieve(frame) && !frame.empty();
}

// 输出按采集节奏产生的帧；不再二次限制为 12 FPS，消费者通过有界缓存防止积压。
void MonitorTask::publishFrame(const cv::Mat &frame)
{
    emit frameReady(frame.clone(), config_.channelId);
}

void MonitorTask::setFileAnalysisBoxes(qint64 pts,quint64 generation,const QList<QRect> &boxes){
    if(!running_||!detection_enabled_||config_.sourceType!=ChannelSourceType::File||generation!=source_generation_||qAbs(source_position_ms_-pts)>500)return;
    detection_boxes_.clear();for(const auto &b:boxes)detection_boxes_.emplace_back(b.x(),b.y(),b.width(),b.height());cached_box_position_ms_=pts;
}
