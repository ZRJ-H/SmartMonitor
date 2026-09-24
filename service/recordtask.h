// 【阅读导引】
// 单路录像 Worker：持有 VideoWriter，接收帧、按固定 FPS 编码并封口普通/事件段。
// 信号返回文件元数据；数据库写入由接收侧服务执行，不能在此直接复用 GUI 数据库连接。

#ifndef RECORDTASK_H
#define RECORDTASK_H

#include "recordingsegment.h"
#include "frameratelimiter.h"
#include "eventrecordingstate.h"
#include <QImage>
#include <QList>
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
// slots 可以被直接调用；只有排队连接/调用才会切换到对象所属线程。
public slots:
    void configure(int channelId, double fps, const QString &directory, int segmentSeconds);
    void saveFrame(const cv::Mat &frame, int channelId);
    void stop();
    void beginEvent(const QString &key, double confidence);
signals:
    void segmentStarted(int channelId, const QString &path);
    void segmentSaved(const Video &video);
    void eventSegmentSaved(const Video &video, const QString &key, double confidence,
                           const QList<QImage> &frames, const QList<qint64> &offsets, bool complete);
    void recordError(int channelId, const QString &message);
    void storageWarning(int channelId, const QString &message);
private:
    EventRecordingState event_; // 当前事件的标识、采样次数与完成阈值。
    double event_confidence_=0; // 触发当前事件时的检测置信度。
    QList<QImage> event_frames_; // 事件样本，像素由各 QImage 独立持有。
    QList<qint64> event_offsets_; // 样本在录像中的偏移，单位毫秒，与图片一一对应。
    bool openWriter(const cv::Mat &frame);
    void closeSegment();
    int channel_id_; // 业务通道号，从 1 开始。
    double fps_; // 实际写入文件的固定帧率，不一定等于源帧率。
    QString directory_; // 录像输出目录。
    int segment_seconds_; // 普通录像分段时长，单位秒。
    bool enabled_; // 配置成功且尚未停止时才允许写帧。
    bool low_space_reported_; // 避免低空间状态下每段都重复报警。
    cv::VideoWriter writer_; // 编码器，仅在录像任务所属线程访问。
    RecordingSegment segment_; // 当前段的路径、开始时间和滚动规则。
    QElapsedTimer elapsed_; // 当前段已运行的单调时间，不受系统时钟校准影响。
    QElapsedTimer frame_clock_; // 输入限流计时基准。
    FrameRateLimiter frame_limiter_; // 限制接收帧的处理频率。
    cv::Size output_size_; // 当前文件固定的编码尺寸。
    qint64 written_frames_ = 0; // 已经写入的总帧数，包含补帧。
    cv::Mat last_encoded_frame_; // 独立保存上一张编码画面，供缺帧时重复写入。
};

#endif // RECORDTASK_H
