#include "monitortask.h"

#include <QtGlobal>

MonitorTask::MonitorTask(QObject *parent)
    : QObject(parent),
      capture_timer_(new QTimer(this)),
      running_(false)
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

    capture_timer_->start(qBound(4, qRound(1000.0 / fps), 1000));
    running_ = true;
    emit sourceReady(config_.channelId, fps);
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
    if (!capture_.read(frame) || frame.empty()) {
        if (config_.sourceType == ChannelSourceType::File) {
            // 文件播放到末尾后回到第一帧，实现持续循环预览。
            capture_.set(cv::CAP_PROP_POS_FRAMES, 0);
            if (capture_.read(frame) && !frame.empty()) {
                // clone 创建独立数据，避免跨线程传递 OpenCV 内部复用的缓冲区。
                emit frameReady(frame.clone(), config_.channelId);
                return;
            }
        }

        emit errorOccurred(config_.channelId,
                           QStringLiteral("读取视频帧失败，当前通道已停止"));
        stopCapture();
        return;
    }

    // 队列信号稍后才会在主线程处理，因此必须发送独立帧副本。
    emit frameReady(frame.clone(), config_.channelId);
}
