// 实现视频源打开、采集节拍控制、文件循环播放和跨线程帧信号发送。
#include "monitortask.h"

#include <QtGlobal>
#include <QFileInfo>
#include <opencv2/imgproc.hpp>

MonitorTask::MonitorTask(QObject *parent)
    : QObject(parent),
      capture_timer_(new QTimer(this)),
      running_(false), detection_enabled_(false), detection_confidence_(0.5),
      event_active_(false), last_event_ms_(0), capture_cadence_(25.0, 12.0),
      output_limiter_(12)
{
    // 精确定时器按视频帧率触发读取；QTimer 会随父对象一起移动到采集线程。
    capture_timer_->setTimerType(Qt::PreciseTimer);
    connect(capture_timer_, &QTimer::timeout,
            this, &MonitorTask::captureNextFrame);
}

MonitorTask::~MonitorTask()
{
    stopCapture();
}

void MonitorTask::configureDetection(bool enabled, const QString &modelPath,
                                     double confidence)
{
    detection_enabled_ = enabled && config_.channelId == 1;
    detection_model_path_ = modelPath.trimmed();
    detection_confidence_ = qBound(0.05, confidence, 0.99);
    detection_net_ = cv::dnn::Net();
    event_active_ = false;
    detection_clock_.invalidate();
    if (!detection_enabled_) return;
    if (!QFileInfo::exists(detection_model_path_)) {
        detection_enabled_ = false;
        emit detectionMessage(config_.channelId,
            QStringLiteral("YOLO 模型文件不存在：%1").arg(detection_model_path_));
        return;
    }
    try {
        detection_net_ = cv::dnn::readNetFromONNX(detection_model_path_.toStdString());
        detection_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        detection_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        emit detectionMessage(config_.channelId, QStringLiteral("智能检测已启用"));
    } catch (const cv::Exception &error) {
        detection_enabled_ = false;
        emit detectionMessage(config_.channelId,
            QStringLiteral("YOLO 模型加载失败：%1").arg(error.what()));
    }
}

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
        opened = capture_.open(config_.filePath.toStdString(), cv::CAP_ANY);
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
    if (fps < 1.0 || fps > 240.0) {
        fps = 25.0;
    }

    capture_cadence_.configure(fps, 12.0);
    if (config_.sourceType == ChannelSourceType::Camera)
        capture_.set(cv::CAP_PROP_FPS, capture_cadence_.outputFps());
    capture_timer_->start(capture_cadence_.timerIntervalMs());
    output_limiter_.reset(config_.channelId - 1);
    output_clock_.restart();
    running_ = true;
    emit sourceReady(config_.channelId, capture_cadence_.outputFps());
    emit captureStateChanged(config_.channelId, true);
}

void MonitorTask::stopCapture()
{
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

    // 队列信号稍后才会在主线程处理，因此必须发送独立帧副本。
    if (detection_enabled_ && !detection_net_.empty()
            && (!detection_clock_.isValid() || detection_clock_.elapsed() >= 500)) {
        detection_clock_.restart();
        bool foundPerson = false;
        double bestConfidence = 0.0;
        try {
            cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
                cv::Size(640, 640), cv::Scalar(), true, false);
            detection_net_.setInput(blob);
            cv::Mat output = detection_net_.forward();
            if (output.dims == 3) {
                cv::Mat rows(output.size[1], output.size[2], CV_32F,
                             output.ptr<float>());
                if (rows.rows < rows.cols) cv::transpose(rows, rows);
                const bool yolo8 = rows.cols == 84;
                for (int i = 0; i < rows.rows; ++i) {
                    const float *data = rows.ptr<float>(i);
                    const double score = yolo8 ? data[4] : data[4] * data[5];
                    if (score < detection_confidence_) continue;
                    const double xScale = frame.cols / 640.0;
                    const double yScale = frame.rows / 640.0;
                    int left = qMax(0, qRound((data[0] - data[2] / 2.0) * xScale));
                    int top = qMax(0, qRound((data[1] - data[3] / 2.0) * yScale));
                    int width = qMin(qRound(data[2] * xScale), frame.cols - left);
                    int height = qMin(qRound(data[3] * yScale), frame.rows - top);
                    if (width > 0 && height > 0)
                        cv::rectangle(frame, cv::Rect(left, top, width, height),
                                      cv::Scalar(0, 255, 0), 2);
                    foundPerson = true;
                    bestConfidence = qMax(bestConfidence, score);
                }
            }
        } catch (const cv::Exception &error) {
            detection_enabled_ = false;
            emit detectionMessage(config_.channelId,
                QStringLiteral("智能检测运行失败，已关闭：%1").arg(error.what()));
        }
        if (foundPerson) {
            last_person_seen_.restart();
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (!event_active_ && now - last_event_ms_ >= 10000) {
                event_active_ = true;
                last_event_ms_ = now;
                emit personDetected(config_.channelId,
                    QStringLiteral("person-%1-%2").arg(config_.channelId).arg(now),
                    QDateTime::currentDateTime(), bestConfidence);
            }
        } else if (event_active_ && last_person_seen_.isValid()
                   && last_person_seen_.elapsed() > 3000) {
            event_active_ = false;
        }
    }
    publishFrame(frame);
}

bool MonitorTask::readNextFrame(cv::Mat &frame)
{
    if (config_.sourceType == ChannelSourceType::Camera)
        return capture_.read(frame) && !frame.empty();

    const int framesToAdvance = capture_cadence_.framesToAdvance();
    for (int i = 0; i < framesToAdvance; ++i) {
        if (capture_.grab()) continue;
        // 文件结束后回到第一帧，实现持续循环预览。
        capture_.set(cv::CAP_PROP_POS_FRAMES, 0);
        if (!capture_.grab()) return false;
    }
    return capture_.retrieve(frame) && !frame.empty();
}

void MonitorTask::publishFrame(const cv::Mat &frame)
{
    if (!output_clock_.isValid()) output_clock_.start();
    if (output_limiter_.shouldPass(config_.channelId - 1, output_clock_.elapsed()))
        emit frameReady(frame.clone(), config_.channelId);
}
