// 【阅读导引】
// 单路采集 Worker：读取视频、按配置执行人形检测，再发送独立帧副本和状态。
// 由 MainWidget 移入采集线程；普通直接调用不会因为 moveToThread 自动切换线程。

// 声明单路视频采集任务；对象运行在独立 QThread 中，并以信号向主线程传递帧和状态。
#ifndef MONITORTASK_H
#define MONITORTASK_H

#include "../model/channelconfig.h"
#include "capturecadence.h"
#include "frameratelimiter.h"

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QElapsedTimer>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/videoio.hpp>

// 声明 Qt 元类型支持；这不是像素深拷贝，跨线程像素独立性仍需 clone() 保证。
Q_DECLARE_METATYPE(cv::Mat)

// 单个通道的采集任务。对象会被移动到独立 QThread，避免读取视频时阻塞界面。
class MonitorTask : public QObject
{
    Q_OBJECT

public:
    explicit MonitorTask(QObject *parent = nullptr);
    ~MonitorTask() override;
    // 只在任务所属线程调用；返回已打开解码器报告的线程数，关闭时为 0。
    int decoderThreadCount() const;

public slots:
    // 打开 config 指定的摄像头或文件，并按源帧率启动定时采集。
    void startCapture(const ChannelConfig &config);
    // 停止定时器并释放当前 OpenCV 数据源；可重复调用。
    void stopCapture();
    void configureCaptureInterval(int milliseconds);
    void configureDetection(bool enabled, const QString &modelPath,
                            double confidence);

private slots:
    // 定时读取下一帧；文件结束时循环播放，读取失败时报告错误并停止。
    void captureNextFrame();

signals:
    // 发送帧的独立副本；channelId 用于主线程定位对应画面窗口。
    void frameReady(const cv::Mat &frame, int channelId);
    // 数据源打开后报告通道号和最终采用的帧率。
    void sourceReady(int channelId, double fps);
    // 通知界面更新指定通道的运行/停止状态。
    void captureStateChanged(int channelId, bool running);
    // 将采集错误及所属通道传回界面线程。
    void errorOccurred(int channelId, const QString &message);
    void detectionMessage(int channelId, const QString &message);
    void personDetected(int channelId, const QString &eventKey,
                        const QDateTime &eventTime, double confidence);

private:
    bool openFileCapture();
    bool readNextFrame(cv::Mat &frame);
    void publishFrame(const cv::Mat &frame);
    cv::VideoCapture capture_; // 当前通道的 OpenCV 数据源句柄。
    // 构造时创建为任务的子对象，随后与任务一起移动到采集线程，并在该线程启动和停止。
    QTimer *capture_timer_;
    ChannelConfig config_;      // 当前正在使用的通道配置快照。
    bool running_;              // 防止重复停止并向外报告当前采集状态。
    bool detection_enabled_;
    QString detection_model_path_;
    double detection_confidence_;
    cv::dnn::Net detection_net_;
    bool event_active_;
    qint64 last_event_ms_;
    QElapsedTimer last_person_seen_;
    QElapsedTimer detection_clock_;
    int capture_interval_ms_ = 40;
    CaptureCadence capture_cadence_;
    FrameRateLimiter output_limiter_;
    QElapsedTimer output_clock_;
};

#endif // MONITORTASK_H
