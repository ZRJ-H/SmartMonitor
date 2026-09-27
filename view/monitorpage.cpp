// 【阅读导引】
// 监控页面：布局、通道选择、画面显示与预览截图。
// 本页拥有自己的控件和状态，通过公开接口及信号与其他模块交互。

#include "monitorpage.h"
#include "../service/imageservice.h"
#include "../util/dbconn.h"
#include "videolabel.h"
#include <QCheckBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

MonitorPage::MonitorPage(QWidget* parent)
    : QFrame(parent)
{
    initUi();
    connect(btn_one, &QPushButton::clicked, this, &MonitorPage::show_single_window);
    connect(btn_four, &QPushButton::clicked, this, &MonitorPage::show_multi_window);
    connect(device_list, &QListWidget::currentItemChanged, this, &MonitorPage::select_channel);
    connect(btn_refresh_cameras, &QPushButton::clicked, this, &MonitorPage::refreshCamerasRequested);
    connect(detection_enabled_box_, &QCheckBox::clicked, this, [this](bool enabled) {
        if (accessEnabled_)
            emit detectionToggleRequested(enabled);
    });
    auto snapshot = new QPushButton("预览截图", this);
    snapshot->setObjectName("previewSnapshotButton");
    findChild<QFrame*>("devicePanel")->layout()->addWidget(snapshot);
    connect(snapshot, &QPushButton::clicked, this, &MonitorPage::take_preview_snapshot);
    setAccessEnabled(false);
}
void MonitorPage::setChannels(const QList<ChannelConfig>& channels)
{
    channel_configs_ = channels;
    refresh_channel_list();
}
void MonitorPage::setAccessEnabled(bool enabled)
{
    accessEnabled_ = enabled;
    btn_one->setEnabled(enabled);
    btn_four->setEnabled(enabled);
    findChild<QPushButton*>("previewSnapshotButton")->setEnabled(enabled);
    detection_enabled_box_->setEnabled(enabled && detectionAllowed_);
    for (int i = 0; i < 4; ++i) {
        latest_source_frames_[i].release();
        video_windows[i]->clear();
        video_windows[i]->setText(enabled ? QString("%1\n%2").arg(channel_configs_.value(i).name,
                                                preview_running_[i] ? "正在接收画面" : "通道未启动")
                                          : QString("请先登录"));
    }
}
void MonitorPage::setDetectionState(bool enabled, bool allowed)
{
    detectionAllowed_ = allowed;
    const QSignalBlocker blocker(detection_enabled_box_);
    detection_enabled_box_->setChecked(allowed && enabled);
    detection_enabled_box_->setEnabled(accessEnabled_ && allowed);
}
void MonitorPage::showMessage(const QString& message)
{
    lab_storage_warning->setText(message);
}
void MonitorPage::setChannelMessage(int channel, const QString& message)
{
    if (channel >= 1 && channel <= 4)
        video_windows[channel - 1]->setText(message);
}
bool MonitorPage::require_permission(AuthPermission, const QString&)
{
    return accessEnabled_;
}
// 包装不可变 OpenCV 像素；调用方保留 Mat 引用直到 QImage 消费结束。
QImage MonitorPage::frameImage(const cv::Mat& frame)
{
    if (frame.empty() || frame.depth() != CV_8U)
        return { };
    QImage::Format format;
    switch (frame.channels()) {
    case 1:
        format = QImage::Format_Grayscale8;
        break;
    case 3:
        format = QImage::Format_BGR888;
        break;
    case 4:
        format = QImage::Format_ARGB32;
        break; // Windows BGRA 字节顺序
    default:
        return { };
    }
    // QImage/QPixmap 在同尺寸时可能共享像素；让最后一个图像引用负责释放 Mat。
    // 使用 const 数据构造也保证 Qt 如需写入会自行分离。
    auto* pixels = new cv::Mat(frame);
    return QImage(
        static_cast<const uchar*>(pixels->data), pixels->cols, pixels->rows,
        static_cast<qsizetype>(pixels->step), format,
        [](void* owner) { delete static_cast<cv::Mat*>(owner); }, pixels);
}

// 创建监控预览页面；控件名称与对象归属保持原约定。
void MonitorPage::initUi()
{
    // ==================== 第 0 页：监控页面 ====================
    QWidget* monitor_page = this;
    monitor_page->setObjectName("contentArea");
    QHBoxLayout* content_layout = new QHBoxLayout(monitor_page);
    content_layout->setContentsMargins(24, 20, 24, 24);
    content_layout->setSpacing(20);

    QFrame* device_panel = new QFrame;
    device_panel->setObjectName("devicePanel");
    device_panel->setMinimumWidth(190);
    device_panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    QVBoxLayout* device_layout = new QVBoxLayout(device_panel);
    device_layout->setContentsMargins(20, 18, 20, 18);
    device_layout->setSpacing(12);

    QLabel* lab_devices = new QLabel("设备列表");
    lab_devices->setObjectName("sectionTitle");
    device_layout->addWidget(lab_devices);

    QFrame* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setObjectName("separator");
    device_layout->addWidget(line);

    device_list = new QListWidget;
    device_list->setObjectName("deviceList");
    device_layout->addWidget(device_list);

    btn_refresh_cameras = new QPushButton("刷新摄像头");
    btn_refresh_cameras->setObjectName("refreshCamerasButton");
    btn_refresh_cameras->setMinimumHeight(34);
    device_layout->addWidget(btn_refresh_cameras);
    content_layout->addWidget(device_panel, 1);

    QFrame* monitor_panel = new QFrame;
    monitor_panel->setObjectName("monitorPanel");
    QVBoxLayout* monitor_layout = new QVBoxLayout(monitor_panel);
    monitor_layout->setContentsMargins(0, 0, 0, 12);
    monitor_layout->setSpacing(10);

    // QGridLayout 在监控页面内部负责单画面和 2×2 四画面的切换。
    QFrame* video_area = new QFrame;
    video_area->setObjectName("videoArea");
    video_layout = new QGridLayout(video_area);
    video_layout->setContentsMargins(0, 0, 0, 0);
    video_layout->setSpacing(3);
    video_layout->setRowStretch(0, 1);
    video_layout->setRowStretch(1, 1);
    video_layout->setColumnStretch(0, 1);
    video_layout->setColumnStretch(1, 1);

    for (int i = 0; i < 4; ++i) {
        video_windows[i] = new VideoLabel(QStringLiteral("通道 %1：等待视频源").arg(i + 1));
        video_windows[i]->setObjectName("videoWindow");
        video_windows[i]->setAlignment(Qt::AlignCenter);
        video_windows[i]->setMinimumSize(180, 150);
        video_layout->addWidget(video_windows[i], i / 2, i % 2);
    }

    lab_video_hint = video_windows[0];
    monitor_layout->addWidget(video_area, 1);

    QHBoxLayout* control_layout = new QHBoxLayout;
    control_layout->setContentsMargins(18, 0, 18, 0);
    control_layout->setSpacing(12);

    btn_one = new QPushButton("单画面");
    btn_four = new QPushButton("四画面");
    detection_enabled_box_ = new QCheckBox("全局人形检测");
    detection_enabled_box_->setObjectName("globalDetectionSwitch");
    detection_enabled_box_->setToolTip("统一控制四路 YOLO 人形检测；模型和置信度在设置页配置。");
    const QList<QPushButton*> control_buttons = { btn_one, btn_four };
    for (QPushButton* button : control_buttons) {
        if (button->objectName().isEmpty()) {
            button->setObjectName("controlButton");
        }
        button->setMinimumHeight(38);
    }

    control_layout->addWidget(btn_one);
    control_layout->addWidget(btn_four);
    control_layout->addStretch();
    control_layout->addWidget(detection_enabled_box_);

    monitor_layout->addLayout(control_layout);
    lab_storage_warning = new QLabel;
    lab_storage_warning->setStyleSheet("color:#ffb74d;font-weight:bold;");
    lab_storage_warning->setWordWrap(true);
    monitor_layout->addWidget(lab_storage_warning);
    content_layout->addWidget(monitor_panel, 3);
}

// 只显示选中通道并更新检测开关；切换布局不销毁其他通道任务。
void MonitorPage::show_single_window()
{
    single_mode_ = true;
    video_layout->setRowStretch(0, 1);
    video_layout->setRowStretch(1, 0);
    video_layout->setColumnStretch(0, 1);
    video_layout->setColumnStretch(1, 0);
    for (int i = 0; i < 4; ++i) {
        video_layout->removeWidget(video_windows[i]);
        video_windows[i]->hide();
    }

    const int index = selected_channel_ - 1;
    video_layout->addWidget(video_windows[index], 0, 0);
    video_windows[index]->show();
}

// 恢复四宫格并关闭单画面检测条件；已有事件录像仍按自己的状态结束。
void MonitorPage::show_multi_window()
{
    single_mode_ = false;
    video_layout->setRowStretch(0, 1);
    video_layout->setRowStretch(1, 1);
    video_layout->setColumnStretch(0, 1);
    video_layout->setColumnStretch(1, 1);
    // i / 2 是行号，i % 2 是列号，形成 2×2 网格。
    for (int i = 0; i < 4; ++i) {
        video_layout->removeWidget(video_windows[i]);
        video_layout->addWidget(video_windows[i], i / 2, i % 2);
        video_windows[i]->show();
    }
}

// 按配置和运行标志刷新设备列表，保持编号与数组下标对应。
void MonitorPage::refresh_channel_list()
{
    // 刷新运行状态不属于用户换通道，避免重载当前 YOLO 并重置事件状态。
    const QSignalBlocker selectionBlocker(device_list);
    const int previousRow = device_list->currentRow();
    device_list->clear();
    for (int i = 0; i < channel_configs_.size(); ++i) {
        const ChannelConfig& config = channel_configs_.at(i);
        const QString source = config.sourceType == ChannelSourceType::Camera
            ? QStringLiteral("摄像头 %1").arg(config.cameraIndex)
            : (config.filePath.isEmpty() ? QStringLiteral("未选择视频") : QStringLiteral("本地视频"));
        const QString state = preview_running_[i] ? QStringLiteral(" · 运行中") : QString();
        device_list->addItem(QStringLiteral("%1 · %2%3").arg(config.name, source, state));
    }
    const int row = previousRow >= 0 ? previousRow : selected_channel_ - 1;
    device_list->setCurrentRow(qBound(0, row, 3));
}

// 把列表选中项转为业务通道号，更新当前控制目标。
void MonitorPage::select_channel(QListWidgetItem* item)
{
    const int row = device_list->row(item);
    if (row < 0 || row >= 4) {
        return;
    }
    selected_channel_ = row + 1;
    lab_video_hint = video_windows[row];
    // 选择只改变显示布局，不重载模型或重置后台检测状态。
    if (single_mode_)
        show_single_window();
}

// 只在 GUI 线程显示信箱交来的最新帧；保留只读源帧供截图，隐藏时跳过绘制。
void MonitorPage::display_frame(const cv::Mat& frame, int channelId)
{
    if (!accessEnabled_ || channelId < 1 || channelId > 4 || frame.empty()
        || !preview_running_[channelId - 1])
        return;
    const int index = channelId - 1;
    // 保留源帧供全分辨率截图使用，平时不做全帧深复制。
    latest_source_frames_[index] = frame;
    QLabel* target = video_windows[index];
    if (!target->isVisible() || window()->isMinimized())
        return;
    const QImage image = frameImage(frame);
    if (image.isNull())
        return;
    // 缩小之后再创建 QPixmap，减少全尺寸转换与拷贝。
    target->setPixmap(
        QPixmap::fromImage(image.scaled(target->size(), Qt::KeepAspectRatio, Qt::FastTransformation)));
    emit previewFramePresented(channelId);
}

// 显示对应通道的具体采集错误，并更新列表状态。
void MonitorPage::show_capture_error(int channelId, const QString& message)
{
    if (channelId < 1 || channelId > 4) {
        return;
    }
    preview_running_[channelId - 1] = false;
    video_windows[channelId - 1]->setText(
        QStringLiteral("%1\n%2").arg(channel_configs_.at(channelId - 1).name, message));
    refresh_channel_list();
}

// 同步采集运行标记；停止时请求录像封口，清空用于截图的旧帧。
void MonitorPage::on_capture_state_changed(int channelId, bool running)
{
    if (channelId < 1 || channelId > 4) {
        return;
    }
    preview_running_[channelId - 1] = running;
    if (!running) {
        latest_source_frames_[channelId - 1].release();
        video_windows[channelId - 1]->setText(QStringLiteral("%1\n%2").arg(
            channel_configs_.at(channelId - 1).name, accessEnabled_ ? "预览已停止" : "请先登录"));
    }
    refresh_channel_list();
}

// 从选中通道保存最近源帧；当前录像尚未入库时不编造 videoId。
void MonitorPage::take_preview_snapshot()
{
    if (!require_permission(AuthPermission::Settings, "保存截图"))
        return;
    const int index = selected_channel_ - 1;
    FeatureImage saved;
    QString error;
    ImageService service(DbConn::getInstance()->database(), image_directory_);
    if (!service.saveManualImage(frameImage(latest_source_frames_[index]).copy(), selected_channel_,
            QDateTime::currentDateTime(), 0, 0, saved, error)) {
        lab_storage_warning->setText(error);
        return;
    }
    emit auditRequested("截图", saved.imagePath);
    lab_storage_warning->setText("截图已保存到图片管理");
}
