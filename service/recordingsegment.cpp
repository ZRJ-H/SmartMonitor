// 【阅读导引】
// 录像段状态：管理时长、命名和起止信息，生成 Video 元数据；不实际编码视频。

#include "recordingsegment.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
RecordingSegment::RecordingSegment(qint64 durationMs)
    : duration_ms_(qMax<qint64>(1000, durationMs)),
      active_(false),
      channel_id_(0)
{
}

// 更改滚动阈值，单位毫秒；实际写文件由 RecordTask 负责。
void RecordingSegment::setDurationMs(qint64 durationMs)
{
    duration_ms_ = qMax<qint64>(1000, durationMs);
}

// 建立新段的通道、起点和路径；遇到已有同名文件时避免覆盖。
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

// 仅在段处于活跃状态且经过时间达到阈值时要求滚动。
bool RecordingSegment::shouldRoll(qint64 elapsedMs) const
{
    return active_ && elapsedMs >= duration_ms_;
}

// 结束当前段并返回元数据；调用者负责先封口编码器并提供实际文件大小。
Video RecordingSegment::finish(const QDateTime &endTime, qint64 fileSize)
{
    const Video video(file_name_, file_path_, channel_id_, start_time_,
                      endTime, fileSize);
    active_ = false;
    return video;
}

// 查询当前是否已经 begin 且尚未 finish。
bool RecordingSegment::isActive() const { return active_; }
// 返回当前段文件名，不含目录。
QString RecordingSegment::fileName() const { return file_name_; }
// 返回当前段完整存储路径。
QString RecordingSegment::filePath() const { return file_path_; }
