// 【阅读导引】
// 录像段状态：管理时长、命名和起止信息，生成 Video 元数据；不实际编码视频。

#ifndef RECORDINGSEGMENT_H
#define RECORDINGSEGMENT_H

#include "../model/video.h"

class RecordingSegment
{
public:
    explicit RecordingSegment(qint64 durationMs = 10000);

    void setDurationMs(qint64 durationMs);
    void begin(const QString &directory, int channelId,
               const QDateTime &startTime);
    bool shouldRoll(qint64 elapsedMs) const;
    Video finish(const QDateTime &endTime, qint64 fileSize);
    bool isActive() const;
    QString fileName() const;
    QString filePath() const;

private:
    qint64 duration_ms_;
    bool active_;
    QString file_name_;
    QString file_path_;
    int channel_id_;
    QDateTime start_time_;
};

#endif // RECORDINGSEGMENT_H
