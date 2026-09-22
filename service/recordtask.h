#ifndef RECORDTASK_H
#define RECORDTASK_H

#include "recordingsegment.h"
#include "frameratelimiter.h"
#include <QElapsedTimer>
#include <QObject>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

class RecordTask : public QObject
{
    Q_OBJECT
public:
    explicit RecordTask(QObject *parent = nullptr);
    ~RecordTask() override;
public slots:
    void configure(int channelId, double fps, const QString &directory, int segmentSeconds);
    void saveFrame(const cv::Mat &frame, int channelId);
    void stop();
signals:
    void segmentStarted(int channelId, const QString &path);
    void segmentSaved(const Video &video);
    void recordError(int channelId, const QString &message);
    void storageWarning(int channelId, const QString &message);
private:
    bool openWriter(const cv::Mat &frame);
    void closeSegment();
    int channel_id_;
    double fps_;
    QString directory_;
    int segment_seconds_;
    bool enabled_;
    bool low_space_reported_;
    cv::VideoWriter writer_;
    RecordingSegment segment_;
    QElapsedTimer elapsed_;
    QElapsedTimer frame_clock_;
    FrameRateLimiter frame_limiter_;
    cv::Size output_size_;
};

#endif // RECORDTASK_H
