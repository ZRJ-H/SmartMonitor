// 【阅读导引】
// 工作线程：采集、录像、文件分析的连接、启动与安全退出。
// 仅依赖后台服务和值对象，不访问窗口、控件或登录会话。

#include "monitoringcontroller.h"
#include "resourcemonitor.h"
#include "../util/dbconn.h"
#include "channelconfigpolicy.h"
#include "monitortask.h"
#include "recordtask.h"
#include "sourceanalysisworker.h"
#include "storageworker.h"
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

MonitoringController::MonitoringController(QObject* parent)
    : QObject(parent)
{
    setObjectName("monitoringController");

    recovery_directory_ = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                              .filePath("pending-media");
}
MonitoringController::~MonitoringController()
{
    shutdown();
}
void MonitoringController::initialize()
{
    init_workers();
    apply_gpu_preference();
    if (!shutting_down_workers_ && !resource_monitor_) {
        resource_monitor_ = new ResourceMonitor(this);
        resource_monitor_->setObjectName("processResourceMonitor");
        connect(resource_monitor_, &ResourceMonitor::usageReady,
            this, &MonitoringController::resourceUsageReady);
        resource_monitor_->start();
    }
}
void MonitoringController::shutdown()
{
    delete resource_monitor_; // Destructor stops and joins before releasing its PDH query.
    resource_monitor_ = nullptr;
    preview_delivery_enabled_.store(false);
    if (!monitor_threads_[0])
        return;
    shutdown_workers();
}
void MonitoringController::setChannels(const QList<ChannelConfig>& channels)
{
    const bool initialized = monitor_threads_[0] != nullptr;
    if (initialized)
        for (int i = 0; i < 4; ++i)
            stop_channel(i);
    channel_configs_ = channels;
    if (initialized)
        start_configured_previews();
}
void MonitoringController::setStorage(const SystemConfig& config)
{
    record_directory_ = config.videoDirectory;
    image_directory_ = QDir(config.imageDirectory).absolutePath();
    record_segment_seconds_value_ = config.segmentSeconds;
    capture_interval_value_ = config.captureIntervalMs;
    if (storage_worker_)
        storage_worker_->setDirectories(recovery_directory_, image_directory_);
    for (int i = 0; i < 4; ++i)
        if (preview_running_[i]) {
            stop_channel(i);
            start_channel(i);
        }
}
void MonitoringController::setDetection(bool enabled, const QString& model, double confidence)
{
    detection_enabled_value_ = enabled;
    yolo_model_path_ = model;
    detection_confidence_value_ = confidence;
    apply_detection_mode();
}
void MonitoringController::setGpuEnabled(bool enabled)
{
    if (shutting_down_workers_ || gpu_enabled_ == enabled) return;
    gpu_enabled_ = enabled;
    apply_gpu_preference();
    emit gpuPreferenceChanged(enabled);
    emit warning(enabled ? QStringLiteral("已请求 GPU 加速；检测启用后显示实际后端，不可用时回退 CPU。")
                         : QStringLiteral("已请求 CPU 检测；切换在当前推理结束后生效。"));
}
void MonitoringController::apply_gpu_preference()
{
    const bool enabled = gpu_enabled_;
    if (source_worker_)
        QMetaObject::invokeMethod(source_worker_, [worker=source_worker_, enabled] {
            worker->setGpuEnabled(enabled);
        }, Qt::QueuedConnection);
    for (auto task : monitor_tasks_)
        if (task) QMetaObject::invokeMethod(task, [task, enabled] {
            task->setGpuEnabled(enabled);
        }, Qt::QueuedConnection);
}
void MonitoringController::requestRecovery()
{
    if (storage_worker_)
        storage_worker_->requestReplay();
}
void MonitoringController::listSourceEvents(int page, int channel, bool history)
{
    if (source_worker_)
        QMetaObject::invokeMethod(
            source_worker_,
            [worker = source_worker_, page, channel, history] { worker->listEvents(page, channel, history); },
            Qt::QueuedConnection);
}
void MonitoringController::resolveSourcePlayback(const SourceEventRow& row, quint64 request)
{
    if (source_worker_)
        QMetaObject::invokeMethod(
            source_worker_,
            [worker = source_worker_, row, request] { worker->resolvePlayback(row, request); },
            Qt::QueuedConnection);
}
void MonitoringController::relocateSource(const QString& asset, const QString& path)
{
    if (source_worker_)
        QMetaObject::invokeMethod(
            source_worker_, [worker = source_worker_, asset, path] { worker->relocate(asset, path); },
            Qt::QueuedConnection);
}
// 创建每路采集/录像线程及对象，绑定生命周期和帧信箱；真正业务在各接收线程运行。
void MonitoringController::init_workers()
{
    if (monitor_threads_[0] || shutting_down_workers_)
        return;
    storage_worker_ = new StorageWorker(StorageConnection::from(DbConn::getInstance()->database()), this);
    storage_worker_->setDirectories(recovery_directory_, image_directory_);
    connect(storage_worker_, &StorageWorker::warning, this,
        [this](const QString& error) { emit warning(error); });
    storage_worker_->start();
    source_thread_ = new QThread(this);
    const QString indexDirectory = qEnvironmentVariable("SMART_MONITOR_INDEX_DIR",
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("file-index"));
    source_worker_ = new SourceAnalysisWorker(indexDirectory);
    source_worker_->moveToThread(source_thread_);
    connect(source_thread_, &QThread::finished, source_worker_, &QObject::deleteLater);
    connect(source_worker_, &SourceAnalysisWorker::message, this, [this](int channel, const QString& text) {
        emit warning(channel > 0 ? QString("通道 %1：%2").arg(channel).arg(text) : text);
        qInfo().noquote() << "Source analysis:" << channel << text;
    });
    connect(source_worker_, &SourceAnalysisWorker::inferenceCompleted, this, [this](int channel, qint64 ms) {
        if (detection_enabled_value_ && detection_allowed_ && !shutting_down_workers_)
            emit detectionCycleCompleted(channel, ms);
    });
    connect(
        source_worker_, &SourceAnalysisWorker::eventsReady, this, &MonitoringController::sourceEventsReady);
    connect(source_worker_, &SourceAnalysisWorker::playbackResolved, this,
        &MonitoringController::sourcePlaybackResolved);
    source_thread_->start();
    // cv::Mat 需要注册后才能作为参数通过 Qt 的跨线程队列信号传递。
    qRegisterMetaType<QList<QImage>>();
    qRegisterMetaType<QList<qint64>>();
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<Video>("Video");
    for (int i = 0; i < 4; ++i) {
        monitor_threads_[i] = new QThread(this);
        monitor_tasks_[i] = new MonitorTask;
        monitor_tasks_[i]->moveToThread(monitor_threads_[i]);
        record_threads_[i] = new QThread(this);
        record_tasks_[i] = new RecordTask;
        record_tasks_[i]->moveToThread(record_threads_[i]);

        connect(monitor_threads_[i], &QThread::finished, monitor_tasks_[i], &QObject::deleteLater);
        connect(record_threads_[i], &QThread::finished, record_tasks_[i], &QObject::deleteLater);
        connect(monitor_tasks_[i], &MonitorTask::sourcePosition, source_worker_,
            &SourceAnalysisWorker::requestPosition, Qt::DirectConnection);
        connect(
            source_worker_, &SourceAnalysisWorker::boxesReady, monitor_tasks_[i],
            [task = monitor_tasks_[i], i](
                int channel, qint64 pts, quint64 generation, const QList<QRect>& boxes) {
                if (channel == i + 1)
                    task->setFileAnalysisBoxes(pts, generation, boxes);
            },
            Qt::QueuedConnection);
        connect(
            record_tasks_[i], &RecordTask::segmentSaved, storage_worker_,
            [worker = storage_worker_](const Video& v) { worker->submit(v); }, Qt::DirectConnection);
        connect(
            record_tasks_[i], &RecordTask::eventSegmentSaved, storage_worker_,
            [worker = storage_worker_](const Video& v, const QString& key, double c,
                const QList<QImage>& frames, const QList<qint64>& offsets,
                bool complete) { worker->submit(v, key, c, frames, offsets, complete); },
            Qt::DirectConnection);
        // 每个消费者最多排队一条通知；慢消费者取最新帧，避免积压全尺寸图像。
        // [this, i] 只捕获控制器和通道号；控制器销毁前必须完成全部线程收尾。
        // 外层回调在采集线程立即执行，invokeMethod 的排队回调才在接收对象所属线程执行。
        // DirectConnection 仅访问带锁邮箱，GUI/录像仍在各自线程消费。
        connect(
            monitor_tasks_[i], &MonitorTask::frameReady, this,
            [this, i](const cv::Mat& frame, int) {
                // 未登录时不向 GUI 排队图像，录像消费者仍正常接收帧。
                if (!preview_delivery_enabled_.load())
                    return;
                if (preview_mailboxes_[i].publish(frame)) {
                    QMetaObject::invokeMethod(
                        this,
                        [this, i] {
                            const cv::Mat latest = preview_mailboxes_[i].take();
                            if (!latest.empty() && preview_delivery_enabled_.load()
                                && !shutting_down_workers_)
                                emit frameReady(latest, i + 1);
                        },
                        Qt::QueuedConnection);
                }
            },
            Qt::DirectConnection);
        auto recorder = record_tasks_[i];
        connect(
            monitor_tasks_[i], &MonitorTask::frameReady, recorder,
            [this, i, recorder](const cv::Mat& frame, int) {
                if (!recording_enabled_ || !record_channel_enabled_[i].load())
                    return;
                if (recording_mailboxes_[i].publish(frame)) {
                    QMetaObject::invokeMethod(
                        recorder,
                        [this, i, recorder] {
                            const cv::Mat latest = recording_mailboxes_[i].take();
                            if (!latest.empty())
                                recorder->saveFrame(latest, i + 1);
                        },
                        Qt::QueuedConnection);
                }
            },
            Qt::DirectConnection);
        connect(monitor_tasks_[i], &MonitorTask::sourceReady, this, &MonitoringController::on_source_ready,
            Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::captureStateChanged, this,
            &MonitoringController::on_capture_state_changed, Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::errorOccurred, this,
            &MonitoringController::show_capture_error, Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::personDetected, this,
            &MonitoringController::on_person_detected, Qt::QueuedConnection);
        connect(
            monitor_tasks_[i], &MonitorTask::detectionCycleCompleted, this,
            [this](int channel, qint64 elapsed) {
                // 关闭开关时，已进入 GUI 队列的旧统计也不能再对外发布。
                if (!shutting_down_workers_ && detection_allowed_ && detection_enabled_value_)
                    emit detectionCycleCompleted(channel, elapsed);
            },
            Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::detectionMessage, this,
            &MonitoringController::on_detection_message, Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::segmentStarted, this,
            &MonitoringController::on_segment_started, Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::segmentSaved, this, &MonitoringController::on_segment_saved,
            Qt::QueuedConnection);
        connect(
            record_tasks_[i], &RecordTask::eventSegmentSaved, this,
            [this](const Video& video, const QString& key, double confidence, const QList<QImage>& frames,
                const QList<qint64>& offsets, bool complete) {
                Q_UNUSED(key);
                Q_UNUSED(confidence);
                Q_UNUSED(frames);
                Q_UNUSED(offsets);
                Q_UNUSED(complete);
                const int index = video.getChannelId() - 1;
                if (index >= 0 && index < 4 && active_record_paths_[index] == video.getVideoPath())
                    active_record_paths_[index].clear();
            },
            Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::recordError, this, &MonitoringController::on_record_error,
            Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::storageWarning, this,
            &MonitoringController::on_storage_warning, Qt::QueuedConnection);

        monitor_threads_[i]->start();
        record_threads_[i]->start();
    }
}

// 先阻止重复收尾，再停采集、封口录像、处理保存通知，最后释放线程和帧信箱。
void MonitoringController::shutdown_workers()
{
    if (shutting_down_workers_)
        return;
    shutting_down_workers_ = true;
    if (source_worker_)
        source_worker_->cancel();
    // 先同步停止采集并释放设备，再退出线程，避免对象销毁时仍在读取帧。
    for (int i = 0; i < 4; ++i) {
        if (monitor_tasks_[i] && monitor_threads_[i] && monitor_threads_[i]->isRunning()) {
            QMetaObject::invokeMethod(monitor_tasks_[i], "stopCapture", Qt::BlockingQueuedConnection);
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (monitor_threads_[i] && monitor_threads_[i]->isRunning()) {
            monitor_threads_[i]->quit();
            monitor_threads_[i]->wait();
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (record_tasks_[i] && record_threads_[i] && record_threads_[i]->isRunning())
            QMetaObject::invokeMethod(record_tasks_[i], "stop", Qt::BlockingQueuedConnection);
    }
    // stop() 发出的最后一个 segmentSaved 是排队信号；对象销毁前让主线程完成入库。
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);
    for (int i = 0; i < 4; ++i) {
        if (record_threads_[i] && record_threads_[i]->isRunning()) {
            record_threads_[i]->quit();
            record_threads_[i]->wait();
        }
        monitor_tasks_[i] = nullptr;
        record_tasks_[i] = nullptr;
        delete monitor_threads_[i];
        monitor_threads_[i] = nullptr;
        delete record_threads_[i];
        record_threads_[i] = nullptr;
        preview_mailboxes_[i].clear();
        recording_mailboxes_[i].clear();
        preview_running_[i] = false;
        active_record_paths_[i].clear();
    }
    if (source_thread_) {
        source_thread_->quit();
        source_thread_->wait();
        delete source_thread_;
        source_thread_ = nullptr;
        source_worker_ = nullptr;
    }
    if (storage_worker_) {
        storage_worker_->finish();
        storage_worker_->wait();
        delete storage_worker_;
        storage_worker_ = nullptr;
    }
    shutting_down_workers_ = false;
}

// 接收有效源 FPS，把通道、目录与时长排队传给对应录像 Worker。
void MonitoringController::on_source_ready(int channelId, double fps)
{
    if (shutting_down_workers_ || channelId < 1 || channelId > 4)
        return;
    if (!recording_enabled_ || !record_tasks_[channelId - 1]
        || channel_configs_.at(channelId - 1).sourceType != ChannelSourceType::Camera)
        return;
    RecordTask* task = record_tasks_[channelId - 1];
    const QString directory = record_directory_;
    const int seconds = record_segment_seconds_value_;
    QMetaObject::invokeMethod(
        task,
        [task, channelId, fps, directory, seconds]() { task->configure(channelId, fps, directory, seconds); },
        Qt::QueuedConnection);
}

// 普通段封口后先写恢复清单再入库；失败保留文件和待办并显示警告。
void MonitoringController::on_segment_saved(const Video& video)
{
    if (video.getChannelId() >= 1 && video.getChannelId() <= 4
        && active_record_paths_[video.getChannelId() - 1] == video.getVideoPath())
        active_record_paths_[video.getChannelId() - 1].clear();
}

// 显示录像失败信息，不把失败文件误报为已完成记录。
void MonitoringController::on_record_error(int channelId, const QString& message)
{
    qWarning().noquote() << QStringLiteral("通道 %1 录像告警：%2").arg(channelId).arg(message);
    emit warning(message);
}

// 将空间不足作为系统事件记录并显示，管理员 ID 不归属当前偶然登录用户。
void MonitoringController::on_storage_warning(int channelId, const QString& message)
{
    emit auditRequested("磁盘告警", QString("通道%1：%2").arg(channelId).arg(message), 0);
    qWarning().noquote() << message;
    emit warning(message);
}

// 记录当前正在写入的文件路径，供通道状态与封口回调核对。
void MonitoringController::on_segment_started(int channelId, const QString& path)
{
    if (channelId >= 1 && channelId <= 4)
        active_record_paths_[channelId - 1] = path;
}

// 任一启用检测的摄像头通道均可触发事件，与页面布局和登录无关。
void MonitoringController::on_person_detected(
    int channelId, const QString& eventKey, const QDateTime& eventTime, double confidence)
{
    Q_UNUSED(eventTime);
    if (shutting_down_workers_ || !recording_enabled_ || !detection_allowed_ || !detection_enabled_value_
        || channelId < 1 || channelId > 4)
        return;
    if (channel_configs_.at(channelId - 1).sourceType != ChannelSourceType::Camera)
        return;
    auto task = record_tasks_[channelId - 1];
    if (!task)
        return;
    QMetaObject::invokeMethod(
        task, [task, eventKey, confidence] { task->beginEvent(eventKey, confidence); }, Qt::QueuedConnection);
}

// 展示模型加载/运行状态；检测不可用不等同于采集必然失败。
void MonitoringController::on_detection_message(int channelId, const QString& message)
{
    if (!detection_allowed_ || !detection_enabled_value_)
        return;
    qInfo().noquote() << QStringLiteral("通道 %1：%2").arg(channelId).arg(message);
    emit warning(message);
}

// 启动具有有效配置的通道；单路配置不完整不应阻止其他路。
void MonitoringController::start_configured_previews()
{
    if (shutting_down_workers_)
        return;
    // 各通道分别校验和启动；一个通道未配置不会影响其他通道。
    for (int i = 0; i < channel_configs_.size(); ++i) {
        QString errorText;
        if (ChannelConfigPolicy::validateForStart(channel_configs_.at(i), errorText)) {
            start_channel(i);
        } else {
            emit channelMessage(i + 1, QStringLiteral("%1\n%2").arg(channel_configs_.at(i).name, errorText));
        }
    }
}

// 以值捕获配置快照，排队在采集线程中设置节拍和打开输入；不直接在 GUI 读视频。
void MonitoringController::start_channel(int index)
{
    if (shutting_down_workers_)
        return;
    if (index < 0 || index >= channel_configs_.size()) {
        return;
    }
    QString errorText;
    const ChannelConfig config = channel_configs_.at(index);
    record_channel_enabled_[index].store(config.sourceType == ChannelSourceType::Camera);
    if (config.sourceType == ChannelSourceType::File && record_tasks_[index])
        QMetaObject::invokeMethod(record_tasks_[index], "stop", Qt::QueuedConnection);
    if (!ChannelConfigPolicy::validateForStart(config, errorText)) {
        emit operationFailed("无法启动预览", errorText);
        return;
    }

    emit channelMessage(index + 1, QStringLiteral("%1\n正在连接...").arg(config.name));
    MonitorTask* task = monitor_tasks_[index];
    if (!task)
        return;
    // 排队到对应工作线程执行，主线程只负责发起请求而不会阻塞。
    QMetaObject::invokeMethod(
        task,
        [task, config, interval = capture_interval_value_]() {
            task->configureCaptureInterval(interval);
            task->startCapture(config);
        },
        Qt::QueuedConnection);
    configure_file_analysis(index);
    const bool detectionEnabled = detection_allowed_ && detection_enabled_value_;
    const QString modelPath = yolo_model_path_;
    const double confidence = detection_confidence_value_;
    QMetaObject::invokeMethod(
        task,
        [task, detectionEnabled, modelPath, confidence]() {
            task->configureDetection(detectionEnabled, modelPath, confidence);
        },
        Qt::QueuedConnection);
}

// 分别向采集和录像线程发停止请求；排队投递不表示此刻已经停止完成。
void MonitoringController::stop_channel(int index)
{
    if (index < 0 || index >= 4 || monitor_tasks_[index] == nullptr) {
        return;
    }
    record_channel_enabled_[index].store(false);
    if (source_worker_)
        QMetaObject::invokeMethod(
            source_worker_,
            [worker = source_worker_, index] { worker->configure(index + 1, { }, false, { }, 0.5); },
            Qt::QueuedConnection);
    QMetaObject::invokeMethod(monitor_tasks_[index], "stopCapture", Qt::QueuedConnection);
    if (record_tasks_[index])
        QMetaObject::invokeMethod(record_tasks_[index], "stop", Qt::QueuedConnection);
}

// 把全局开关和模型参数同步到所有通道，布局与选中项不改变检测范围。
void MonitoringController::apply_detection_mode()
{
    for (int i = 0; i < 4; ++i) {
        configure_file_analysis(i);
        auto task = monitor_tasks_[i];
        if (!task)
            continue;
        const bool enabled = detection_allowed_ && detection_enabled_value_;
        const QString path = yolo_model_path_;
        const double confidence = detection_confidence_value_;
        QMetaObject::invokeMethod(
            task, [task, enabled, path, confidence] { task->configureDetection(enabled, path, confidence); },
            Qt::QueuedConnection);
    }
}

void MonitoringController::configure_file_analysis(int index)
{
    if (!source_worker_ || index < 0 || index >= channel_configs_.size())
        return;
    const auto config = channel_configs_.at(index);
    const bool enabled
        = detection_allowed_ && detection_enabled_value_ && config.sourceType == ChannelSourceType::File;
    const QString path = config.sourceType == ChannelSourceType::File ? config.filePath : QString();
    QMetaObject::invokeMethod(
        source_worker_,
        [worker = source_worker_, channel = index + 1, path, enabled, model = yolo_model_path_,
            confidence = detection_confidence_value_] {
            worker->configure(channel, path, enabled, model, confidence);
        },
        Qt::QueuedConnection);
}

void MonitoringController::show_capture_error(int channel, const QString& message)
{
    if (channel < 1 || channel > 4)
        return;
    preview_running_[channel - 1] = false;
    emit captureError(channel, message);
}
void MonitoringController::on_capture_state_changed(int channel, bool running)
{
    if (channel < 1 || channel > 4)
        return;
    running = running && !shutting_down_workers_;
    preview_running_[channel - 1] = running;
    if (!running && record_tasks_[channel - 1])
        QMetaObject::invokeMethod(record_tasks_[channel - 1], "stop", Qt::QueuedConnection);
    emit captureStateChanged(channel, running);
}
