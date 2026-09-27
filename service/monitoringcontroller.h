#ifndef MONITORINGCONTROLLER_H
#define MONITORINGCONTROLLER_H
#include "latestframebuffer.h"
#include "sourceeventrow.h"
#include <QObject>
class SourceAnalysisWorker;
#include "../model/channelconfig.h"
#include "../model/video.h"
#include "systemconfigservice.h"
class QThread;
class MonitorTask;
class RecordTask;
class StorageWorker;
// 拥有运行状态、帧缓冲与工作线程。登录仅控制帧投递，不控制后台采集。
class MonitoringController : public QObject {
    Q_OBJECT
public:
    explicit MonitoringController(QObject* parent = nullptr);
    ~MonitoringController() override;
    void initialize();
    void shutdown();
    bool detectionAllowed() const
    {
        return detection_allowed_;
    }
    void setPreviewEnabled(bool enabled)
    {
        preview_delivery_enabled_.store(enabled);
    }
public slots:
    void setChannels(const QList<ChannelConfig>& channels);
    void setStorage(const SystemConfig& config);
    void setDetection(bool enabled, const QString& model, double confidence);
    void start_configured_previews();
    void start_channel(int index);
    void stop_channel(int index);
    void requestRecovery();
    void listSourceEvents(int page, int channel, bool history);
    void resolveSourcePlayback(const SourceEventRow& row, quint64 request);
    void relocateSource(const QString& asset, const QString& path);
    void on_capture_state_changed(int channel, bool running);
    void on_person_detected(int channel, const QString& key, const QDateTime& time, double confidence);
signals:
    void frameReady(const cv::Mat& frame, int channel);
    void channelMessage(int channel, const QString& message);
    void captureError(int channel, const QString& message);
    void captureStateChanged(int channel, bool running);
    void warning(const QString& message);
    void operationFailed(const QString& title, const QString& message);
    void auditRequested(const QString& action, const QString& detail, int actorId);
    void detectionCycleCompleted(int channel, qint64 elapsed);
    void sourceEventsReady(const QList<SourceEventRow>& rows, int page, int total);
    void sourcePlaybackResolved(const SourceEventRow& row, quint64 request, const QString& error);

private:
    void init_workers();
    void shutdown_workers();
    void apply_detection_mode();
    void configure_file_analysis(int index);
    void on_source_ready(int, double);
    void on_segment_saved(const Video&);
    void on_record_error(int, const QString&);
    void on_storage_warning(int, const QString&);
    void on_segment_started(int, const QString&);
    void on_detection_message(int, const QString&);
    void show_capture_error(int, const QString&);
    const QString pipeline_mode_ = qEnvironmentVariable("SMART_MONITOR_PIPELINE", "full");
    const bool recording_enabled_ = pipeline_mode_ != "preview",
               detection_allowed_ = pipeline_mode_ == "full";
    QList<ChannelConfig> channel_configs_;
    QString record_directory_, image_directory_, recovery_directory_, yolo_model_path_;
    int record_segment_seconds_value_ = 10, capture_interval_value_ = 40;
    bool detection_enabled_value_ = false, shutting_down_workers_ = false;
    double detection_confidence_value_ = 0.5;
    std::atomic_bool preview_delivery_enabled_ { false }, record_channel_enabled_[4] { };
    LatestFrameBuffer preview_mailboxes_[4], recording_mailboxes_[4];
    SourceAnalysisWorker* source_worker_ = nullptr;
    QThread* source_thread_ = nullptr;
    StorageWorker* storage_worker_ = nullptr;
    QThread *monitor_threads_[4] { }, *record_threads_[4] { };
    MonitorTask* monitor_tasks_[4] { };
    RecordTask* record_tasks_[4] { };
    QString active_record_paths_[4];
    bool preview_running_[4] { };
};
#endif
