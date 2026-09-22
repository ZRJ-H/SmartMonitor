#ifndef VIDEO_H
#define VIDEO_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

class Video
{
public:
    Video(const QString &videoName = QString(),
          const QString &videoPath = QString(),
          int channelId = 0,
          const QDateTime &startTime = QDateTime(),
          const QDateTime &endTime = QDateTime(),
          qint64 fileSize = 0,
          int id = 0);

    int getId() const;
    QString getVideoName() const;
    QString getVideoPath() const;
    int getChannelId() const;
    QDateTime getStartTime() const;
    QDateTime getEndTime() const;
    qint64 getFileSize() const;

private:
    int id_;
    QString video_name_;
    QString video_path_;
    int channel_id_;
    QDateTime start_time_;
    QDateTime end_time_;
    qint64 file_size_;
};

Q_DECLARE_METATYPE(Video)

#endif // VIDEO_H
