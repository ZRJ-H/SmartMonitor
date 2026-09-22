#include "video.h"

Video::Video(const QString &videoName,
             const QString &videoPath,
             int channelId,
             const QDateTime &startTime,
             const QDateTime &endTime,
             qint64 fileSize,
             int id)
    : id_(id),
      video_name_(videoName),
      video_path_(videoPath),
      channel_id_(channelId),
      start_time_(startTime),
      end_time_(endTime),
      file_size_(fileSize)
{
}

int Video::getId() const { return id_; }
QString Video::getVideoName() const { return video_name_; }
QString Video::getVideoPath() const { return video_path_; }
int Video::getChannelId() const { return channel_id_; }
QDateTime Video::getStartTime() const { return start_time_; }
QDateTime Video::getEndTime() const { return end_time_; }
qint64 Video::getFileSize() const { return file_size_; }
