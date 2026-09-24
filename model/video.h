// 【阅读导引】
// 录像元数据值对象：保存路径、通道、时间和大小，不持有视频帧或编码器。
// 可通过信号跨线程传递；构造出的对象不代表已经成功写入数据库。

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
