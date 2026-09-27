#include "mainwidget.h"
#include "../service/maintenancecontroller.h"
#include "../service/monitoringcontroller.h"
#include "../service/operationlogservice.h"
#include "../util/dbconn.h"
#include "apptheme.h"
#include "imagepage.h"
#include "monitorpage.h"
#include "operationlogpage.h"
#include "playbackpage.h"
#include "settingspage.h"
#include <QButtonGroup>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
MainWidget::MainWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("mainWidget");
    setWindowTitle("监控系统主页面");
    resize(1280, 760);
    setMinimumSize(960, 600);
    init_ui();
    AppTheme::applyMain(this);
    init_showtime();
    monitoring_ = new MonitoringController(this);
    maintenance_ = new MaintenanceController(this);
    image_page_ = new ImagePage(
        DbConn::getInstance()->database(), QDir(settings_page_->storage().imageDirectory).absolutePath());
    log_page_ = new OperationLogPage(DbConn::getInstance()->database());
    page_stack->addWidget(image_page_);
    page_stack->addWidget(log_page_);
    init_connect();
    if (!settings_page_->startupMessage().isEmpty())
        monitor_page_->showMessage(settings_page_->startupMessage());
    monitoring_->setChannels(settings_page_->channels());
    monitor_page_->setChannels(settings_page_->channels());
    monitoring_->setStorage(settings_page_->storage());
    updateImageDirectory(settings_page_->storage().imageDirectory);
    monitoring_->setDetection(
        settings_page_->detectionEnabled(), settings_page_->modelPath(), settings_page_->confidence());
    monitor_page_->setDetectionState(settings_page_->detectionEnabled(), monitoring_->detectionAllowed());
    update_management_access();
    monitoring_->initialize();
    maintenance_->start();
    if (qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") != "0")
        QTimer::singleShot(0, monitoring_, &MonitoringController::start_configured_previews);
}
MainWidget::~MainWidget()
{
    maintenance_->stop();
    monitoring_->shutdown();
}
void MainWidget::updateImageDirectory(const QString& path)
{
    const QString absolute = QDir(path).absolutePath();
    image_page_->setImageDirectory(absolute);
    monitor_page_->setImageDirectory(absolute);
    playback_page_->setImageDirectory(absolute);
}
void MainWidget::init_connect()
{
    connect(btn_monitor_page, &QPushButton::clicked, this, &MainWidget::show_monitor_page);
    connect(btn_playback_page, &QPushButton::clicked, this, &MainWidget::show_playback_page);
    connect(btn_setting_page, &QPushButton::clicked, this, &MainWidget::show_setting_page);
    connect(btn_login, &QPushButton::clicked, this, &MainWidget::handle_login_button);
    connect(images_button_, &QPushButton::clicked, this, [this] {
        if (require_permission(AuthPermission::Settings, "管理图片")) {
            image_page_->reload();
            page_stack->setCurrentWidget(image_page_);
            images_button_->setChecked(true);
        }
    });
    connect(logs_button_, &QPushButton::clicked, this, [this] {
        if (require_permission(AuthPermission::Settings, "查看日志")) {
            log_page_->reload();
            page_stack->setCurrentWidget(log_page_);
            logs_button_->setChecked(true);
        }
    });
    connect(image_page_, &ImagePage::playbackRequested, playback_page_, &PlaybackPage::open_linked_video);
    connect(
        log_page_, &OperationLogPage::playbackRequested, playback_page_, &PlaybackPage::open_linked_video);
    connect(log_page_, &OperationLogPage::eventImagesRequested, this, [this](qint64 id) {
        if (require_permission(AuthPermission::Settings, "查看事件图片")) {
            image_page_->showEventImages(id);
            page_stack->setCurrentWidget(image_page_);
            images_button_->setChecked(true);
        }
    });
    connect(image_page_, &ImagePage::imagesExported, this,
        [this](int n) { audit("导出图片", QString::number(n)); });
    connect(image_page_, &ImagePage::imagesDeleted, this,
        [this](int n) { audit("删除图片", QString::number(n)); });
    connect(playback_page_, &PlaybackPage::showRequested, this, [this] {
        page_stack->setCurrentWidget(playback_page_);
        btn_playback_page->setChecked(true);
    });
    connect(playback_page_, &PlaybackPage::auditRequested, this,
        [this](const QString& a, const QString& d) { audit(a, d); });
    connect(monitor_page_, &MonitorPage::auditRequested, this,
        [this](const QString& a, const QString& d) { audit(a, d); });
    connect(settings_page_, &SettingsPage::auditRequested, this, &MainWidget::audit);
    connect(monitoring_, &MonitoringController::auditRequested, this, &MainWidget::audit);
    connect(maintenance_, &MaintenanceController::auditRequested, this, &MainWidget::audit);
    connect(monitor_page_, &MonitorPage::refreshCamerasRequested, settings_page_,
        &SettingsPage::refresh_camera_devices);
    connect(monitor_page_, &MonitorPage::detectionToggleRequested, settings_page_,
        &SettingsPage::setDetectionEnabled);
    connect(settings_page_, &SettingsPage::channelsSaved, monitor_page_, &MonitorPage::setChannels);
    connect(settings_page_, &SettingsPage::channelsSaved, monitoring_, &MonitoringController::setChannels);
    connect(settings_page_, &SettingsPage::storageSaved, monitoring_, &MonitoringController::setStorage);
    connect(settings_page_, &SettingsPage::storageSaved, this,
        [this](const SystemConfig& c) { updateImageDirectory(c.imageDirectory); });
    connect(settings_page_, &SettingsPage::detectionSaved, monitoring_, &MonitoringController::setDetection);
    connect(
        settings_page_, &SettingsPage::detectionSaved, this, [this](bool enabled, const QString&, double) {
            monitor_page_->setDetectionState(enabled, monitoring_->detectionAllowed());
        });
    connect(settings_page_, &SettingsPage::warning, monitor_page_, &MonitorPage::showMessage);
    connect(settings_page_, &SettingsPage::detectionToggleRejected, this,
        [this](bool enabled) { monitor_page_->setDetectionState(enabled, monitoring_->detectionAllowed()); });
    connect(monitoring_, &MonitoringController::warning, monitor_page_, &MonitorPage::showMessage);
    connect(monitoring_, &MonitoringController::operationFailed, this,
        [this](const QString& title, const QString& message) {
            QMessageBox::information(this, title, message);
        });
    connect(monitoring_, &MonitoringController::frameReady, monitor_page_, &MonitorPage::display_frame);
    connect(monitoring_, &MonitoringController::captureStateChanged, monitor_page_,
        &MonitorPage::on_capture_state_changed);
    connect(
        monitoring_, &MonitoringController::captureError, monitor_page_, &MonitorPage::show_capture_error);
    connect(
        monitoring_, &MonitoringController::channelMessage, monitor_page_, &MonitorPage::setChannelMessage);
    connect(monitor_page_, &MonitorPage::previewFramePresented, this, &MainWidget::previewFramePresented);
    connect(monitoring_, &MonitoringController::detectionCycleCompleted, this,
        &MainWidget::detectionCycleCompleted);
    connect(playback_page_, &PlaybackPage::sourceEventsRequested, monitoring_,
        &MonitoringController::listSourceEvents);
    connect(playback_page_, &PlaybackPage::sourcePlaybackRequested, monitoring_,
        &MonitoringController::resolveSourcePlayback);
    connect(playback_page_, &PlaybackPage::sourceRelocationRequested, monitoring_,
        &MonitoringController::relocateSource);
    connect(monitoring_, &MonitoringController::sourceEventsReady, playback_page_,
        &PlaybackPage::receiveSourceEvents);
    connect(monitoring_, &MonitoringController::sourcePlaybackResolved, playback_page_,
        &PlaybackPage::receiveSourcePlayback);
    connect(settings_page_, &SettingsPage::backupRequested, this, [this] {
        if (require_permission(AuthPermission::Settings, "备份数据库"))
            maintenance_->startBackup(auth_session.userId());
    });
    connect(maintenance_, &MaintenanceController::backupRunningChanged, settings_page_,
        &SettingsPage::setBackupRunning);
    connect(maintenance_, &MaintenanceController::message, monitor_page_, &MonitorPage::showMessage);
    connect(maintenance_, &MaintenanceController::recoveryRequested, monitoring_,
        &MonitoringController::requestRecovery);
}
void MainWidget::update_management_access()
{
    const bool allowed = auth_session.isLoggedIn();
    for (auto button : { images_button_, logs_button_, btn_playback_page, btn_setting_page })
        button->setEnabled(allowed);
    monitor_page_->setAccessEnabled(allowed);
    playback_page_->setAccessEnabled(allowed);
    settings_page_->setAccessEnabled(allowed);
    image_page_->setEnabled(allowed);
    log_page_->setEnabled(allowed);
}
void MainWidget::audit(const QString& action, const QString& detail, int actorId)
{
    OperationLogService service(DbConn::getInstance()->database());
    QString error;
    if (!service.record(actorId < 0 ? auth_session.userId() : actorId, action, detail,
            QDateTime::currentDateTime(), error))
        monitor_page_->showMessage("操作成功，审计日志写入失败：" + error);
}
// 创建控件和布局并保存成员指针；业务信号连接主要在 init_connect 中建立。
void MainWidget::init_ui()
{
    // 整个窗口采用垂直布局：上方导航栏，下方 QStackedWidget 页面区。
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // -------------------- 顶部导航区 --------------------
    QFrame* top_bar = new QFrame;
    top_bar->setObjectName("topBar");
    top_bar->setFixedHeight(118);

    QHBoxLayout* top_layout = new QHBoxLayout(top_bar);
    top_layout->setContentsMargins(36, 16, 30, 12);
    top_layout->setSpacing(16);

    QLabel* lab_title = new QLabel("安防监控系统管理平台");
    lab_title->setObjectName("titleLabel");
    top_layout->addWidget(lab_title);
    top_layout->addStretch();

    // 这三个按钮保存为成员变量，稍后分别连接到三个页面切换槽。
    btn_monitor_page = new QPushButton("监控");
    btn_playback_page = new QPushButton("回放");
    btn_setting_page = new QPushButton("设置");

    const QList<QPushButton*> nav_buttons = { btn_monitor_page, btn_playback_page, btn_setting_page };

    // QButtonGroup 只管理选中状态的互斥；真正的页面显示由 QStackedWidget 管理。
    QButtonGroup* nav_group = new QButtonGroup(this);
    nav_group->setExclusive(true);

    for (QPushButton* button : nav_buttons) {
        button->setObjectName("navButton");
        button->setCheckable(true);
        button->setMinimumSize(90, 46);
        nav_group->addButton(button);
        top_layout->addWidget(button);
    }
    images_button_ = new QPushButton("图片");
    logs_button_ = new QPushButton("日志");
    images_button_->setObjectName("imagesNavigation");
    logs_button_->setObjectName("logsNavigation");
    for (auto button : { images_button_, logs_button_ }) {
        button->setCheckable(true);
        nav_group->addButton(button);
        button->setMinimumSize(65, 46);
        top_layout->addWidget(button);
    }
    btn_monitor_page->setChecked(true);

    // lab_time 会被 QTimer 每隔一秒更新，因此必须保存为成员变量。
    lab_time = new QLabel;
    lab_time->setObjectName("timeLabel");
    lab_time->setMinimumWidth(210);
    lab_time->setAlignment(Qt::AlignCenter);
    top_layout->addWidget(lab_time);

    QFrame* login_box = new QFrame;
    login_box->setObjectName("loginBox");
    login_box->setFixedWidth(88);
    QVBoxLayout* login_layout = new QVBoxLayout(login_box);
    login_layout->setContentsMargins(4, 0, 4, 0);
    login_layout->setSpacing(4);

    btn_login = new QPushButton("登录");
    btn_login->setObjectName("loginButton");
    btn_login->setFixedSize(66, 66);

    lab_login = new QLabel("未登录");
    lab_login->setObjectName("loginLabel");
    lab_login->setAlignment(Qt::AlignCenter);

    login_layout->addWidget(btn_login, 0, Qt::AlignHCenter);
    login_layout->addWidget(lab_login);
    top_layout->addWidget(login_box);
    main_layout->addWidget(top_bar);

    // QStackedWidget 像一叠卡片：各业务页面依次加入，一次只显示其中一个。
    page_stack = new QStackedWidget;
    page_stack->setObjectName("pageStack");
    main_layout->addWidget(page_stack, 1);

    // 页面顺序固定：监控=0、回放=1、设置=2。
    monitor_page_ = new MonitorPage;
    page_stack->addWidget(monitor_page_);
    playback_page_ = new PlaybackPage;
    page_stack->addWidget(playback_page_);
    settings_page_ = new SettingsPage;
    page_stack->addWidget(settings_page_);
    page_stack->setCurrentIndex(0);
}

// 创建 GUI 定时器更新顶部时钟，与采集线程的定时器职责不同。
void MainWidget::init_showtime()
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(update_time()));
    m_timer->start();

    // 窗口刚显示时立即刷新，避免前一秒时间标签为空。
    update_time();
}

// 切换到监控页，页面切换不等于重新创建采集任务。
void MainWidget::show_monitor_page()
{
    // 索引 0 对应第一个 addWidget() 加入的监控页面。
    page_stack->setCurrentIndex(0);
}

// 先检查回放权限，再切换页面并加载录像列表。
void MainWidget::show_playback_page()
{
    if (!require_permission(AuthPermission::Playback, "查看录像回放")) {
        btn_monitor_page->setChecked(true);
        return;
    }
    page_stack->setCurrentIndex(1);
}

// 检查设置权限后显示配置控件，不在切页时写配置。
void MainWidget::show_setting_page()
{
    if (!require_permission(AuthPermission::Settings, "修改系统设置")) {
        btn_monitor_page->setChecked(true);
        return;
    }
    page_stack->setCurrentIndex(2);
}

// 根据现有会话决定发起登录还是退出登录。
void MainWidget::handle_login_button()
{
    if (auth_session.isLoggedIn()) {
        logout();
        return;
    }
    emit login_requested();
}

void MainWidget::setLoggedInUser(int userId, const QString& name)
{
    if (userId < 1 || name.trimmed().isEmpty())
        return;
    auth_session.login(userId, name);
    monitoring_->setPreviewEnabled(true);
    update_management_access();
    audit("登录", "管理员登录成功");
    lab_login->setText(name);
    btn_login->setText("退出");
    playback_page_->load_video_records();
}

// 统一检查会话权限；失败时显示说明，由调用者立即返回。
bool MainWidget::require_permission(AuthPermission permission, const QString& actionName)
{
    if (auth_session.canAccess(permission)) {
        return true;
    }

    QMessageBox::information(
        this, "需要登录", QString("请先登录，再%1。登录后才能查看监控画面或进行操作。").arg(actionName));
    return false;
}

void MainWidget::logout()
{
    audit("退出", "管理员退出");
    monitoring_->setPreviewEnabled(false);
    auth_session.logout();
    update_management_access();
    lab_login->setText("未登录");
    btn_login->setText("登录");
    btn_monitor_page->setChecked(true);
    page_stack->setCurrentIndex(0);
    emit login_requested();
}

// 刷新可读的当前日期时间，不用它衡量采集耗时。
void MainWidget::update_time()
{
    lab_time->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
}
