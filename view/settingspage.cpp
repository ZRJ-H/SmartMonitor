// 【阅读导引】
// 设置页面：通道配置、设备枚举、录像和检测设置。
// 本页拥有自己的控件和状态，通过公开接口及信号与其他模块交互。

#include "settingspage.h"
#include "../service/channelconfigpolicy.h"
#include "../service/systemconfigservice.h"
#include "../util/dbconn.h"
#include <QCameraDevice>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStorageInfo>
#include <QStringList>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget* parent)
    : QFrame(parent)
{
    const auto startupCapture = connect(
        this, &SettingsPage::warning, this, [this](const QString& text) { startupWarnings_ << text; });
    media_devices_ = new QMediaDevices(this);
    initUi();
    load_system_settings();
    load_channel_configs();
    refresh_camera_devices();
    apply_channel_configs_to_ui();
    SystemConfigService service(DbConn::getInstance()->database());
    QString error;
    if (!service.load(storage_, error))
        emit warning(error);
    record_directory_edit_->setText(storage_.videoDirectory);
    int row = record_segment_seconds_->findData(storage_.segmentSeconds);
    if (row < 0) {
        record_segment_seconds_->addItem(
            QString("%1 秒").arg(storage_.segmentSeconds), storage_.segmentSeconds);
        row = record_segment_seconds_->count() - 1;
    }
    record_segment_seconds_->setCurrentIndex(row);
    image_directory_edit_->setText(storage_.imageDirectory);
    connect(media_devices_, &QMediaDevices::videoInputsChanged, this, &SettingsPage::refresh_camera_devices);
    connect(btn_save_channel_configs, &QPushButton::clicked, this, &SettingsPage::save_channel_configs);
    connect(btn_save_recording_settings, &QPushButton::clicked, this, &SettingsPage::save_recording_settings);
    connect(btn_save_detection_settings, &QPushButton::clicked, this, &SettingsPage::save_detection_settings);
    for (int i = 0; i < 4; ++i) {
        connect(channel_source_boxes_[i], &QComboBox::currentIndexChanged, this,
            [this, i] { update_source_controls(i); });
        connect(channel_file_buttons_[i], &QPushButton::clicked, this, [this, i] { choose_channel_file(i); });
    }
    backupButton_ = new QPushButton("立即备份数据库", this);
    backupButton_->setObjectName("backupDatabaseButton");
    layout()->addWidget(backupButton_);
    connect(backupButton_, &QPushButton::clicked, this, [this] {
        if (accessEnabled_)
            emit backupRequested();
    });
    disconnect(startupCapture);
    setAccessEnabled(false);
}
bool SettingsPage::require_permission(AuthPermission, const QString& action)
{
    if (accessEnabled_)
        return true;
    QMessageBox::information(this, "需要登录", "请先登录，再" + action + "。");
    return false;
}
void SettingsPage::setDetectionEnabled(bool enabled)
{
    if (!accessEnabled_)
        return;
    if (enabled && !QFileInfo::exists(yolo_model_path_)) {
        emit warning("启用检测前，请在设置页选择有效的 ONNX 模型。");
        emit detectionToggleRejected(detection_enabled_value_);
        return;
    }
    if (!config_model_.upsertValue("detection.enabled", enabled ? "1" : "0")) {
        emit warning("检测开关保存失败，请检查数据库连接。");
        emit detectionToggleRejected(detection_enabled_value_);
        return;
    }
    detection_enabled_value_ = enabled;
    emit detectionSaved(enabled, yolo_model_path_, detection_confidence_value_);
    emit warning(enabled ? "已请求开启四路人形检测" : "四路人形检测已关闭");
    emit auditRequested("修改配置", enabled ? "开启全局人形检测" : "关闭全局人形检测");
}
// 创建通道与系统设置页面；控件名称与对象归属保持原约定。
void SettingsPage::initUi()
{
    // ==================== 第 2 页：设置页面 ====================
    QWidget* setting_page = this;
    setting_page->setObjectName("settingPage");
    QVBoxLayout* setting_layout = new QVBoxLayout(setting_page);
    setting_layout->setContentsMargins(28, 22, 28, 28);
    setting_layout->setSpacing(18);

    QLabel* setting_title = new QLabel("系统设置");
    setting_title->setObjectName("pageTitle");
    setting_layout->addWidget(setting_title);

    QFrame* setting_card = new QFrame;
    setting_card->setObjectName("pageCard");
    QVBoxLayout* setting_card_layout = new QVBoxLayout(setting_card);
    setting_card_layout->setContentsMargins(28, 24, 28, 24);
    setting_card_layout->setSpacing(18);
    setting_card_layout->setSizeConstraint(QLayout::SetMinimumSize);

    QLabel* setting_hint = new QLabel("配置四个通道的名称和视频来源。摄像头不能被多个通道重复占用。");
    setting_hint->setObjectName("settingText");
    setting_hint->setWordWrap(true);
    setting_card_layout->addWidget(setting_hint);

    QGridLayout* config_grid = new QGridLayout;
    config_grid->setHorizontalSpacing(12);
    config_grid->setVerticalSpacing(10);
    const QStringList headers = { "通道", "名称", "来源", "摄像头", "本地视频", "" };
    for (int column = 0; column < headers.size(); ++column) {
        QLabel* header = new QLabel(headers.at(column));
        header->setObjectName("settingText");
        config_grid->addWidget(header, 0, column);
    }

    for (int i = 0; i < 4; ++i) {
        QLabel* channel_label = new QLabel(QString::number(i + 1));
        channel_label->setObjectName("settingText");

        channel_name_edits_[i] = new QLineEdit;
        channel_name_edits_[i]->setObjectName(QStringLiteral("channelNameEdit_%1").arg(i + 1));

        channel_source_boxes_[i] = new QComboBox;
        channel_source_boxes_[i]->setObjectName(QStringLiteral("channelSourceType_%1").arg(i + 1));
        channel_source_boxes_[i]->addItem("摄像头", static_cast<int>(ChannelSourceType::Camera));
        channel_source_boxes_[i]->addItem("本地视频", static_cast<int>(ChannelSourceType::File));

        channel_camera_boxes_[i] = new QComboBox;
        channel_camera_boxes_[i]->setObjectName(QStringLiteral("channelCamera_%1").arg(i + 1));

        channel_file_edits_[i] = new QLineEdit;
        channel_file_edits_[i]->setObjectName(QStringLiteral("channelFile_%1").arg(i + 1));
        channel_file_edits_[i]->setPlaceholderText("选择本地视频文件");

        channel_file_buttons_[i] = new QPushButton("浏览");
        channel_file_buttons_[i]->setObjectName("controlButton");

        config_grid->addWidget(channel_label, i + 1, 0);
        config_grid->addWidget(channel_name_edits_[i], i + 1, 1);
        config_grid->addWidget(channel_source_boxes_[i], i + 1, 2);
        config_grid->addWidget(channel_camera_boxes_[i], i + 1, 3);
        config_grid->addWidget(channel_file_edits_[i], i + 1, 4);
        config_grid->addWidget(channel_file_buttons_[i], i + 1, 5);
    }
    config_grid->setColumnStretch(1, 1);
    config_grid->setColumnStretch(4, 2);
    setting_card_layout->addLayout(config_grid);

    QGridLayout* recording_grid = new QGridLayout;
    QLabel* recording_title = new QLabel("录像与智能检测");
    recording_title->setObjectName("sectionTitle");
    setting_card_layout->addWidget(recording_title);
    record_directory_edit_ = new QLineEdit;
    QPushButton* browse_record_directory = new QPushButton("浏览目录");
    browse_record_directory->setObjectName("controlButton");
    record_segment_seconds_ = new QComboBox;
    record_segment_seconds_->addItem("10 秒（开发）", 10);
    record_segment_seconds_->addItem("60 秒（验收）", 60);
    recording_grid->addWidget(new QLabel("录像目录"), 0, 0);
    recording_grid->addWidget(record_directory_edit_, 0, 1);
    recording_grid->addWidget(browse_record_directory, 0, 2);
    recording_grid->addWidget(new QLabel("分段时长"), 1, 0);
    recording_grid->addWidget(record_segment_seconds_, 1, 1);
    yolo_model_edit_ = new QLineEdit;
    yolo_model_edit_->setPlaceholderText("选择 YOLO ONNX 模型");
    QPushButton* browse_yolo_model = new QPushButton("浏览模型");
    browse_yolo_model->setObjectName("controlButton");
    detection_confidence_spin_ = new QDoubleSpinBox;
    detection_confidence_spin_->setRange(0.05, 0.99);
    detection_confidence_spin_->setSingleStep(0.05);
    detection_confidence_spin_->setValue(0.5);
    recording_grid->addWidget(new QLabel("检测开关位于监控预览页，统一控制所有通道"), 2, 0, 1, 3);
    recording_grid->addWidget(new QLabel("YOLO 模型"), 3, 0);
    recording_grid->addWidget(yolo_model_edit_, 3, 1);
    recording_grid->addWidget(browse_yolo_model, 3, 2);
    recording_grid->addWidget(new QLabel("置信度阈值"), 4, 0);
    recording_grid->addWidget(detection_confidence_spin_, 4, 1);
    image_directory_edit_ = new QLineEdit;
    image_directory_edit_->setObjectName("imageDirectoryEdit");
    recording_grid->addWidget(new QLabel("图片目录"), 5, 0);
    recording_grid->addWidget(image_directory_edit_, 5, 1, 1, 2);
    recording_grid->addWidget(new QLabel("采集节奏"), 6, 0);
    recording_grid->addWidget(new QLabel("文件按源帧率播放；摄像头按设备输出采集"), 6, 1, 1, 2);

    setting_card_layout->addLayout(recording_grid);
    QHBoxLayout* feature_save_layout = new QHBoxLayout;
    btn_save_recording_settings = new QPushButton("保存录像设置");
    btn_save_detection_settings = new QPushButton("保存检测设置");
    btn_save_recording_settings->setObjectName("saveRecordingSettingsButton");
    btn_save_detection_settings->setObjectName("saveDetectionSettingsButton");
    feature_save_layout->addWidget(btn_save_recording_settings);
    feature_save_layout->addWidget(btn_save_detection_settings);
    feature_save_layout->addStretch();
    setting_card_layout->addLayout(feature_save_layout);
    connect(browse_record_directory, &QPushButton::clicked, this, &SettingsPage::browse_record_directory);
    connect(browse_yolo_model, &QPushButton::clicked, this, &SettingsPage::browse_yolo_model);

    btn_save_channel_configs = new QPushButton("保存通道配置");
    btn_save_channel_configs->setObjectName("saveChannelConfigButton");
    btn_save_channel_configs->setMinimumHeight(40);
    setting_card_layout->addWidget(btn_save_channel_configs, 0, Qt::AlignRight);
    setting_card_layout->addStretch();
    // 设置项较多，小窗口应滚动而不是把输入框和文字压扁。
    auto setting_scroll = new QScrollArea;
    setting_scroll->setObjectName("settingsScrollArea");
    setting_scroll->setWidgetResizable(true);
    setting_scroll->setFrameShape(QFrame::NoFrame);
    setting_scroll->setWidget(setting_card);
    setting_layout->addWidget(setting_scroll, 1);
}

// 加载四路配置；失败保留可用默认值并显示原因，不能把损坏数据直接应用。
void SettingsPage::load_channel_configs()
{
    QString errorText;
    if (!channel_config_service_.load(channel_configs_, errorText)) {
        // 配置损坏或数据库暂时不可用时，仍允许界面使用安全默认值启动。
        channel_configs_ = ChannelConfigPolicy::defaults();
        emit warning(QStringLiteral("配置加载失败，已使用默认值：%1").arg(errorText));
    }
}

// 读取已保存的录像/检测设置并填入控件；兼容配置还有 SystemConfigService 入口。
void SettingsPage::load_system_settings()
{
    QString value;
    bool found = false;
    if (config_model_.findValue("record.directory", value, found) && found && !value.trimmed().isEmpty())
        record_directory_ = value.trimmed();
    if (config_model_.findValue("record.segment_seconds", value, found) && found) {
        const int seconds = value.toInt();
        if (seconds == 10 || seconds == 60)
            record_segment_seconds_value_ = seconds;
    }
    if (config_model_.findValue("detection.enabled", value, found) && found)
        detection_enabled_value_ = value == QStringLiteral("1");
    if (config_model_.findValue("detection.model_path", value, found) && found)
        yolo_model_path_ = value;
    if (config_model_.findValue("detection.confidence", value, found) && found) {
        bool ok = false;
        const double confidence = value.toDouble(&ok);
        if (ok)
            detection_confidence_value_ = qBound(0.05, confidence, 0.99);
    }
    record_directory_edit_->setText(record_directory_);
    int segmentRow = record_segment_seconds_->findData(record_segment_seconds_value_);
    if (segmentRow >= 0)
        record_segment_seconds_->setCurrentIndex(segmentRow);

    yolo_model_edit_->setText(yolo_model_path_);
    detection_confidence_spin_->setValue(detection_confidence_value_);
}

// 选择候选录像目录；取消对话框时保持原值，真正保存由保存按钮触发。
void SettingsPage::browse_record_directory()
{
    const QString path
        = QFileDialog::getExistingDirectory(this, "选择录像保存目录", record_directory_edit_->text());
    if (!path.isEmpty())
        record_directory_edit_->setText(path);
}

// 先验证并事务保存存储/采集参数，再更新内存并重启正在运行的通道。
void SettingsPage::save_recording_settings()
{
    if (!require_permission(AuthPermission::Settings, "修改配置"))
        return;
    QString path = QDir::cleanPath(record_directory_edit_->text().trimmed());
    QDir dir(path);
    if (path.isEmpty() || (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        || !QFileInfo(path).isWritable()) {
        QMessageBox::warning(this, "录像设置", "录像目录无法创建或不可写。");
        return;
    }
    const int seconds = record_segment_seconds_->currentData().toInt();
    SystemConfigService service(DbConn::getInstance()->database());
    SystemConfig config;
    QString error;
    if (!service.load(config, error)) {
        QMessageBox::warning(this, "配置读取失败", error);
        return;
    }
    config.videoDirectory = path;
    config.imageDirectory = image_directory_edit_->text().trimmed();
    config.segmentSeconds = seconds; // 保留旧配置字段，仅兼容数据库，不再控制采集调度。
    if (!service.save(config, error)) {
        QMessageBox::warning(this, "配置保存失败", error);
        return;
    }
    storage_ = config;
    emit storageSaved(storage_);
    emit auditRequested("配置", "存储和采集参数已保存");
    QStorageInfo storage(path);
    storage.refresh();
    if (storage.isValid() && storage.bytesTotal() > 0
        && static_cast<double>(storage.bytesAvailable()) / storage.bytesTotal() <= 0.10) {
        emit warning("录像目录所在磁盘剩余空间不超过 10%，请及时清理。");
        emit auditRequested("磁盘告警", "通道0：录像目录所在磁盘剩余空间不超过 10%，请及时清理。", 0);
    }
    QMessageBox::information(this, "录像设置", "录像目录和分段时长已保存并应用。");
}

// 选择 ONNX 路径填入控件；此时不加载模型、不启用推理。
void SettingsPage::browse_yolo_model()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "选择 YOLO ONNX 模型", yolo_model_edit_->text(), "ONNX 模型 (*.onnx)");
    if (!path.isEmpty())
        yolo_model_edit_->setText(path);
}

// 验证模型文件并成组保存检测参数，成功后才重新配置各通道检测。
void SettingsPage::save_detection_settings()
{
    if (!require_permission(AuthPermission::Settings, "修改配置"))
        return;
    const bool enabled = detection_enabled_value_;
    const QString path = yolo_model_edit_->text().trimmed();
    const double confidence = detection_confidence_spin_->value();
    if (enabled && !QFileInfo::exists(path)) {
        QMessageBox::warning(this, "智能检测", "启用检测前请选择存在的 ONNX 模型文件。");
        return;
    }
    QString error;
    SystemConfigModel model(DbConn::getInstance()->database());
    if (!model.saveAll({ { "detection.enabled", enabled ? "1" : "0" }, { "detection.model_path", path },
                           { "detection.confidence", QString::number(confidence) } },
            error)) {
        QMessageBox::warning(this, "智能检测", error);
        return;
    }
    detection_enabled_value_ = enabled;
    yolo_model_path_ = path;
    detection_confidence_value_ = confidence;
    emit detectionSaved(enabled, path, confidence);
    emit auditRequested("修改配置", "智能检测设置");
    QMessageBox::information(this, "智能检测",
        enabled ? "已为配置的通道启用后台人形检测，退出登录后继续运行。"
                : "智能检测已关闭，预览与普通录像保持运行。");
}

// 把已应用的配置回填控件，避免把编辑中的值当成正在运行的配置。
void SettingsPage::apply_channel_configs_to_ui()
{
    for (int i = 0; i < 4; ++i) {
        const ChannelConfig& config = channel_configs_.at(i);
        channel_name_edits_[i]->setText(config.name);
        channel_source_boxes_[i]->setCurrentIndex(config.sourceType == ChannelSourceType::Camera ? 0 : 1);

        int cameraRow = channel_camera_boxes_[i]->findData(config.cameraIndex);
        if (cameraRow < 0) {
            channel_camera_boxes_[i]->addItem(
                QStringLiteral("摄像头 %1（当前不可用）").arg(config.cameraIndex), config.cameraIndex);
            cameraRow = channel_camera_boxes_[i]->count() - 1;
        }
        channel_camera_boxes_[i]->setCurrentIndex(cameraRow);
        channel_file_edits_[i]->setText(config.filePath);
        update_source_controls(i);
    }
}

// 收集四路候选配置，返回值对象供校验；此函数不写数据库。
QList<ChannelConfig> SettingsPage::collect_channel_configs_from_ui() const
{
    QList<ChannelConfig> configs;
    for (int i = 0; i < 4; ++i) {
        ChannelConfig config;
        config.channelId = i + 1;
        config.name = channel_name_edits_[i]->text().trimmed();
        config.sourceType = static_cast<ChannelSourceType>(channel_source_boxes_[i]->currentData().toInt());
        config.cameraIndex = channel_camera_boxes_[i]->currentData().toInt();
        config.filePath = channel_file_edits_[i]->text().trimmed();
        configs.append(config);
    }
    return configs;
}

// 根据摄像头/文件类型启用相关输入，避免两类来源同时编辑。
void SettingsPage::update_source_controls(int index)
{
    if (index < 0 || index >= 4) {
        return;
    }
    const bool cameraSelected
        = channel_source_boxes_[index]->currentData().toInt() == static_cast<int>(ChannelSourceType::Camera);
    channel_camera_boxes_[index]->setEnabled(cameraSelected);
    channel_file_edits_[index]->setEnabled(!cameraSelected);
    channel_file_buttons_[index]->setEnabled(!cameraSelected);
}

// 为指定通道选择本地文件，取消时保持原路径。
void SettingsPage::choose_channel_file(int index)
{
    if (index < 0 || index >= 4) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("为通道 %1 选择视频文件").arg(index + 1), channel_file_edits_[index]->text(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mkv *.mov);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        channel_file_edits_[index]->setText(path);
    }
}

// 枚举 Qt 可见摄像头并刷新选择项，实际能否打开由采集任务验证。
void SettingsPage::refresh_camera_devices()
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (int i = 0; i < 4; ++i) {
        const int previousIndex = channel_camera_boxes_[i]->currentData().toInt();
        channel_camera_boxes_[i]->clear();
        for (int cameraIndex = 0; cameraIndex < cameras.size(); ++cameraIndex) {
            channel_camera_boxes_[i]->addItem(
                QStringLiteral("%1（索引 %2）").arg(cameras.at(cameraIndex).description()).arg(cameraIndex),
                cameraIndex);
        }
        if (cameras.isEmpty()) {
            channel_camera_boxes_[i]->addItem(QStringLiteral("未检测到摄像头"), -1);
        }
        const int restoredRow = channel_camera_boxes_[i]->findData(previousIndex);
        if (restoredRow >= 0) {
            channel_camera_boxes_[i]->setCurrentIndex(restoredRow);
        }
    }
}

// 校验并保存四路候选配置，成功后应用到通道；失败不冒充保存成功。
void SettingsPage::save_channel_configs()
{
    if (!require_permission(AuthPermission::Settings, "保存通道配置")) {
        return;
    }

    const QList<ChannelConfig> configs = collect_channel_configs_from_ui();
    QString errorText;
    if (!ChannelConfigPolicy::validateAssignments(configs, errorText)) {
        QMessageBox::warning(this, "配置无效", errorText);
        return;
    }
    if (!channel_config_service_.save(configs, errorText)) {
        QMessageBox::warning(this, "保存失败", errorText);
        return;
    }

    channel_configs_ = configs;
    emit channelsSaved(configs);
    QMessageBox::information(this, "保存成功", "四通道名称和来源已保存并应用。");
}

void SettingsPage::setBackupRunning(bool running)
{
    backupButton_->setEnabled(!running);
}
