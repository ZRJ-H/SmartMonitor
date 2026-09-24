// 【阅读导引】
// 单路录像 Worker：持有 VideoWriter，接收帧、按固定 FPS 编码并封口普通/事件段。
// 信号返回文件元数据；数据库写入由接收侧服务执行，不能在此直接复用 GUI 数据库连接。

#include "recordtask.h"
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <opencv2/imgproc.hpp>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
RecordTask::RecordTask(QObject *parent)
    : QObject(parent), channel_id_(0), fps_(25.0), segment_seconds_(10),
      enabled_(false), low_space_reported_(false), segment_(10000),
      frame_limiter_(10)
{
}

// 对象销毁时释放本类负责的资源；依赖的外部对象需按约定保持有效。
RecordTask::~RecordTask() { stop(); }

// 封口旧段并设置通道、有效编码 FPS、目录及分段时长，成功后才允许接收帧。
void RecordTask::configure(int channelId, double fps,
                           const QString &directory, int segmentSeconds)
{
    stop();
    channel_id_ = channelId;
    const double sourceFps = (fps >= 1.0 && fps <= 240.0) ? fps : 25.0;
    fps_ = qMin(10.0, sourceFps);
    directory_ = QDir::cleanPath(directory.trimmed());
    segment_seconds_ = qMax(1, segmentSeconds);
    segment_.setDurationMs(static_cast<qint64>(segment_seconds_) * 1000);
    QDir dir(directory_);
    if ((!dir.exists() && !dir.mkpath(QStringLiteral(".")))
            || !QFileInfo(directory_).isWritable()) {
        emit recordError(channel_id_, QStringLiteral(
            "录像目录不存在且无法创建，或目录不可写：%1").arg(directory_));
        return;
    }
    enabled_ = true;
    output_size_ = cv::Size();
    frame_limiter_.reset(channel_id_ - 1);
    frame_clock_.restart();
}

// 检查空间、生成新路径、选择尺寸与编码器；成功后从零开始计算该段帧数和时间。
bool RecordTask::openWriter(const cv::Mat &frame)
{
    QStorageInfo storage(directory_);
    storage.refresh();
    if (storage.isValid() && storage.isReady() && storage.bytesTotal() > 0) {
        const double ratio = static_cast<double>(storage.bytesAvailable())
                / static_cast<double>(storage.bytesTotal());
        if (ratio <= 0.10 && !low_space_reported_) {
            low_space_reported_ = true;
            emit storageWarning(channel_id_,
                QStringLiteral("录像磁盘剩余空间仅 %1%，请及时清理")
                    .arg(ratio * 100.0, 0, 'f', 1));
        } else if (ratio > 0.10) {
            low_space_reported_ = false;
        }
    }
    segment_.begin(directory_, channel_id_, QDateTime::currentDateTime());
    const double scale = qMin(1.0, qMin(1280.0 / frame.cols, 720.0 / frame.rows));
    // 清除最低二进制位得到偶数尺寸，兼容编码器的尺寸要求。
    int width = qMax(2, static_cast<int>(frame.cols * scale)) & ~1;
    int height = qMax(2, static_cast<int>(frame.rows * scale)) & ~1;
    output_size_ = cv::Size(width, height);
    const int codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    bool opened = writer_.open(segment_.filePath().toStdString(), cv::VideoWriter::fourcc('a','v','c','1'),
                                fps_, output_size_, frame.channels() != 1);
    if(!opened) opened = writer_.open(segment_.filePath().toStdString(), codec, fps_,
                                      output_size_, frame.channels() != 1);
    if (!opened) {
        emit recordError(channel_id_,
            QStringLiteral("无法创建录像文件：%1").arg(segment_.filePath()));
        segment_.finish(QDateTime::currentDateTime(), 0);
        return false;
    }
    emit segmentStarted(channel_id_, segment_.filePath());
    written_frames_ = 0;
    last_encoded_frame_.release();
    elapsed_.restart();
    return true;
}

// 过滤无效/过快输入，到期滚动段；必要时补帧，采集事件图片并写入当前帧。
void RecordTask::saveFrame(const cv::Mat &frame, int channelId)
{
    if (!enabled_ || channelId != channel_id_ || frame.empty()) return;
    if (!frame_clock_.isValid()) frame_clock_.start();
    if (!frame_limiter_.shouldPass(channel_id_ - 1, frame_clock_.elapsed())) return;
    if (writer_.isOpened() && segment_.shouldRoll(elapsed_.elapsed())) closeSegment();
    if (!writer_.isOpened() && !openWriter(frame)) {
        enabled_ = false;
        return;
    }
    // 固定 FPS 文件按帧数决定播放时长；用上一帧补足经过的时间，避免缺帧导致录像加速。
    // 这只能补齐时间轴，不能恢复最新帧缓存覆盖掉的真实画面。
    const qint64 targetIndex = qRound64(elapsed_.elapsed() * fps_ / 1000.0);
    while (!last_encoded_frame_.empty() && written_frames_ < targetIndex) {
        writer_.write(last_encoded_frame_);
        ++written_frames_;
    }
    if(event_.takeSample(elapsed_.elapsed())){
        cv::Mat rgb;
        if(frame.channels()==1)cv::cvtColor(frame,rgb,cv::COLOR_GRAY2RGB);
        else if(frame.channels()==4)cv::cvtColor(frame,rgb,cv::COLOR_BGRA2RGB);
        else cv::cvtColor(frame,rgb,cv::COLOR_BGR2RGB);
        // rgb 是局部 Mat；copy() 让 QImage 拥有独立像素，函数返回后图片仍然有效。
        event_frames_ << QImage(rgb.data,rgb.cols,rgb.rows,static_cast<int>(rgb.step),QImage::Format_RGB888).copy();
        // 图片对应即将写入的帧，偏移按编码帧序号计算，单位毫秒，供回放定位。
        event_offsets_ << qRound64(written_frames_ * 1000.0 / fps_);
    }
    if (frame.size() == output_size_) last_encoded_frame_ = frame.clone();
    else cv::resize(frame, last_encoded_frame_, output_size_, 0.0, 0.0, cv::INTER_AREA);
    writer_.write(last_encoded_frame_);
    ++written_frames_;
}

// 补齐本段时间轴并 release 编码器，检查文件后发出普通或事件保存结果。
void RecordTask::closeSegment()
{
    if (!writer_.isOpened()) return;
    const qint64 capMs = event_.active() ? 60000 : static_cast<qint64>(segment_seconds_) * 1000;
    const qint64 expected = qRound64(qMin(elapsed_.elapsed(),capMs) * fps_ / 1000.0);
    while (!last_encoded_frame_.empty() && written_frames_ < expected) {
        writer_.write(last_encoded_frame_);
        ++written_frames_;
    }
    writer_.release();
    const QFileInfo file(segment_.filePath());
    if (file.exists() && file.size() > 0) {
        const Video video=segment_.finish(QDateTime::currentDateTime(), file.size());
        if(event_.active()){
            emit eventSegmentSaved(video,event_.key(),event_confidence_,event_frames_,event_offsets_,
                                   event_.shouldFinish(elapsed_.elapsed()));
        }else emit segmentSaved(video);
    } else {
        const QString path = segment_.filePath();
        segment_.finish(QDateTime::currentDateTime(), 0);
        emit recordError(channel_id_,
            QStringLiteral("录像文件为空，未写入数据库：%1").arg(path));
    }
    event_.reset();event_frames_.clear();event_offsets_.clear();
    segment_.setDurationMs(static_cast<qint64>(segment_seconds_)*1000);
}

// 封口当前段、清空事件状态并禁用写入；再次停止不会创建额外录像。
void RecordTask::stop()
{
    closeSegment();
    event_.reset();event_frames_.clear();event_offsets_.clear();
    segment_.setDurationMs(static_cast<qint64>(segment_seconds_)*1000);
    enabled_ = false;
}

// 仅在已配置且无活跃事件时生效：先关闭普通段，再设置约 60 秒事件段。
void RecordTask::beginEvent(const QString &key,double confidence)
{
    if(!enabled_ || event_.active() || key.isEmpty())return;
    // 先封口普通段再开启事件段；同一时间只持有一个 VideoWriter。
    closeSegment();
    if(!event_.begin(key))return;
    event_confidence_=confidence;event_frames_.clear();event_offsets_.clear();
    segment_.setDurationMs(60000);
}
