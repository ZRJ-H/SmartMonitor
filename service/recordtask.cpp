#include "recordtask.h"
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <opencv2/imgproc.hpp>

RecordTask::RecordTask(QObject *parent)
    : QObject(parent), channel_id_(0), fps_(25.0), segment_seconds_(10),
      enabled_(false), low_space_reported_(false), segment_(10000),
      frame_limiter_(10)
{
}

RecordTask::~RecordTask() { stop(); }

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
    int width = qMax(2, static_cast<int>(frame.cols * scale)) & ~1;
    int height = qMax(2, static_cast<int>(frame.rows * scale)) & ~1;
    output_size_ = cv::Size(width, height);
    const int codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    if (!writer_.open(segment_.filePath().toStdString(), codec, fps_,
                      output_size_, frame.channels() != 1)) {
        emit recordError(channel_id_,
            QStringLiteral("无法创建录像文件：%1").arg(segment_.filePath()));
        segment_.finish(QDateTime::currentDateTime(), 0);
        return false;
    }
    emit segmentStarted(channel_id_, segment_.filePath());
    elapsed_.restart();
    return true;
}

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
    if (frame.size() == output_size_) {
        writer_.write(frame);
    } else {
        cv::Mat resized;
        cv::resize(frame, resized, output_size_, 0.0, 0.0, cv::INTER_AREA);
        writer_.write(resized);
    }
}

void RecordTask::closeSegment()
{
    if (!writer_.isOpened()) return;
    writer_.release();
    const QFileInfo file(segment_.filePath());
    if (file.exists() && file.size() > 0) {
        emit segmentSaved(segment_.finish(QDateTime::currentDateTime(), file.size()));
    } else {
        const QString path = segment_.filePath();
        segment_.finish(QDateTime::currentDateTime(), 0);
        emit recordError(channel_id_,
            QStringLiteral("录像文件为空，未写入数据库：%1").arg(path));
    }
}

void RecordTask::stop()
{
    closeSegment();
    enabled_ = false;
}
