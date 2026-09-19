#ifndef MONITORTASK_H
#define MONITORTASK_H

#include "../model/channelconfig.h"

#include <QObject>
#include <QTimer>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

Q_DECLARE_METATYPE(cv::Mat)

class MonitorTask : public QObject
{
    Q_OBJECT

public:
    explicit MonitorTask(QObject *parent = nullptr);
    ~MonitorTask() override;

public slots:
    void startCapture(const ChannelConfig &config);
    void stopCapture();

private slots:
    void captureNextFrame();

signals:
    void frameReady(const cv::Mat &frame, int channelId);
    void sourceReady(int channelId, double fps);
    void captureStateChanged(int channelId, bool running);
    void errorOccurred(int channelId, const QString &message);

private:
    cv::VideoCapture capture_;
    QTimer *capture_timer_;
    ChannelConfig config_;
    bool running_;
};

#endif // MONITORTASK_H
