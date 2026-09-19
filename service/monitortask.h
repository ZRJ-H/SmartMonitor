#ifndef MONITORTASK_H
#define MONITORTASK_H

#include "../model/channelconfig.h"

#include <QObject>
#include <QTimer>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

Q_DECLARE_METATYPE(cv::Mat)

// 单个通道的采集任务。对象会被移动到独立 QThread，避免读取视频时阻塞界面。
class MonitorTask : public QObject
{
    Q_OBJECT

public:
    explicit MonitorTask(QObject *parent = nullptr);
    ~MonitorTask() override;

public slots:
    // 这两个槽通过队列连接在采集线程中执行。
    void startCapture(const ChannelConfig &config);
    void stopCapture();

private slots:
    void captureNextFrame();

signals:
    // 信号携带 channelId，使主线程能把结果送到对应的画面窗口。
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
