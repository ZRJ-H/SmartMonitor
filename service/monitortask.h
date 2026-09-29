// 【阅读导引】
// 单路采集 Worker：按源节奏读取视频，检测由独立线程异步执行。
// 由 MainWidget 移入采集线程；普通直接调用不会因为 moveToThread 自动切换线程。

// 声明单路视频采集任务；对象运行在独立 QThread 中，并以信号向主线程传递帧和状态。
#ifndef MONITORTASK_H
#define MONITORTASK_H

#include "../model/channelconfig.h"
#include "capturecadence.h"
#include "frameratelimiter.h"

#include <QObject>
#include <vector>
#include <QTimer>
#include <QRect>
#include <QDateTime>
#include <QElapsedTimer>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/videoio.hpp>

// 声明 Qt 元类型支持；这不是像素深拷贝，跨线程像素独立性仍需 clone() 保证。
Q_DECLARE_METATYPE(cv::Mat)
class QThread;
class DetectionWorker;

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
    void setFileAnalysisBoxes(qint64 pts,quint64 generation,const QList<QRect> &boxes);
    void configureCaptureInterval(int milliseconds);
    void setGpuEnabled(bool enabled); // Queue to the capture thread; never restarts capture.
    void configureDetection(bool enabled, const QString &modelPath,
                            double confidence);

private slots:
    // 定时读取下一帧；文件结束时循环播放，读取失败时报告错误并停止。
    void captureNextFrame();

signals:
    void sourcePosition(int channel,qint64 pts,quint64 generation);
    void detectionCycleCompleted(int channelId, qint64 elapsedMs);
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
    void configureDetectionImpl(bool enabled, const QString &modelPath, double confidence, bool preserveEvent);
    bool gpu_enabled_ = false;
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
    QString detection_backend_;
    double detection_confidence_;
    QThread *detection_thread_;
    DetectionWorker *detection_worker_;
    quint64 detection_generation_ = 0; // 停止/换源/开关变更时递增，用于丢弃旧结果。
    bool detection_pending_ = false; // 每路最多一个推理任务，不堆积过时图像。
    bool detection_ready_ = false;
    std::vector<cv::Rect> detection_boxes_;
    bool event_active_;
    qint64 last_event_ms_;
    QElapsedTimer last_person_seen_;
    QElapsedTimer detection_clock_;
    quint64 source_generation_=0;
    qint64 source_position_ms_=0,cached_box_position_ms_=-1;
    double source_fps_ = 25.0; // 文件元数据帧率；无效时采用 25 FPS。
    qint64 next_frame_index_ = 0; // 跨循环累计的源帧位置。
    QElapsedTimer playback_clock_; // 绝对调度基准，避免处理耗时逐帧累加。
};

#endif // MONITORTASK_H
