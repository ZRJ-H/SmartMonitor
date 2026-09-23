#include "recordingsegment.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

RecordingSegment::RecordingSegment(qint64 durationMs)
    : duration_ms_(qMax<qint64>(1000, durationMs)),
      active_(false),
      channel_id_(0)
{
}

void RecordingSegment::setDurationMs(qint64 durationMs)
{
    duration_ms_ = qMax<qint64>(1000, durationMs);
}

void RecordingSegment::begin(const QString &directory, int channelId,
                             const QDateTime &startTime)
{
    channel_id_ = channelId;
    start_time_ = startTime;
    file_name_ = QStringLiteral("Record_%1_%2.mp4")
            .arg(startTime.toString(QStringLiteral("yyyyMMdd_HHmmss")))
            .arg(channelId);
    file_path_ = QDir(directory).filePath(file_name_);
    // 事件可能在普通段开始的同一秒触发，不能覆盖刚刚封口的文件。
    if(QFileInfo::exists(file_path_)){
        file_name_=QStringLiteral("Record_%1_%2_%3.mp4").arg(startTime.toString("yyyyMMdd_HHmmss")).arg(channelId)
            .arg(QUuid::createUuid().toString(QUuid::Id128).left(12));
        file_path_=QDir(directory).filePath(file_name_);
    }
    active_ = true;
}

bool RecordingSegment::shouldRoll(qint64 elapsedMs) const
{
    return active_ && elapsedMs >= duration_ms_;
}

Video RecordingSegment::finish(const QDateTime &endTime, qint64 fileSize)
{
    const Video video(file_name_, file_path_, channel_id_, start_time_,
                      endTime, fileSize);
    active_ = false;
    return video;
}

bool RecordingSegment::isActive() const { return active_; }
QString RecordingSegment::fileName() const { return file_name_; }
QString RecordingSegment::filePath() const { return file_path_; }
