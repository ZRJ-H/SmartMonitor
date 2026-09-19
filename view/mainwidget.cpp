#include "mainwidget.h"
#include "../service/channelconfigpolicy.h"
#include "../service/monitortask.h"

#include <QButtonGroup>
#include <QCameraDevice>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      btn_monitor_page(nullptr),
      btn_playback_page(nullptr),
      btn_setting_page(nullptr),
      btn_login(nullptr),
      btn_one(nullptr),
      btn_four(nullptr),
      btn_refresh_records(nullptr),
      btn_start_preview(nullptr),
      btn_stop_preview(nullptr),
      btn_refresh_cameras(nullptr),
      btn_save_channel_configs(nullptr),
      lab_login(nullptr),
      lab_time(nullptr),
      lab_video_hint(nullptr),
      page_stack(nullptr),
      device_list(nullptr),
      video_record_list(nullptr),
      lab_video_page(nullptr),
      btn_previous_video_page(nullptr),
      btn_next_video_page(nullptr),
      video_service(5),
      current_video_page(1),
      auth_session(),
      video_layout(nullptr),
      m_timer(nullptr),
      media_devices_(new QMediaDevices(this)),
      selected_channel_(1)
{
    for (int i = 0; i < 4; ++i) {
        video_windows[i] = nullptr;
        channel_name_edits_[i] = nullptr;
        channel_source_boxes_[i] = nullptr;
        channel_camera_boxes_[i] = nullptr;
        channel_file_edits_[i] = nullptr;
        channel_file_buttons_[i] = nullptr;
        monitor_threads_[i] = nullptr;
        monitor_tasks_[i] = nullptr;
        preview_running_[i] = false;
    }

    // objectName 供 QSS 的 QWidget#mainWidget 选择器使用。
    setObjectName("mainWidget");
    setWindowTitle("监控系统主页面");
    resize(1280, 760);
    setMinimumSize(960, 600);

    // 初始化顺序很重要：先创建控件，再设置样式和连接信号，最后启动定时器。
    init_ui();
    init_style();
    init_connect();
    init_showtime();
    init_workers();
    load_channel_configs();
    refresh_camera_devices();
    apply_channel_configs_to_ui();
    QTimer::singleShot(0, this, &MainWidget::start_configured_previews);
}

MainWidget::~MainWidget()
{
    shutdown_workers();
}

void MainWidget::init_ui()
{
    // 整个窗口采用垂直布局：上方导航栏，下方 QStackedWidget 页面区。
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // -------------------- 顶部导航区 --------------------
    QFrame *top_bar = new QFrame;
    top_bar->setObjectName("topBar");
    top_bar->setFixedHeight(118);

    QHBoxLayout *top_layout = new QHBoxLayout(top_bar);
    top_layout->setContentsMargins(36, 16, 30, 12);
    top_layout->setSpacing(16);

    QLabel *lab_title = new QLabel("安防监控系统管理平台");
    lab_title->setObjectName("titleLabel");
    top_layout->addWidget(lab_title);
    top_layout->addStretch();

    // 这三个按钮保存为成员变量，稍后分别连接到三个页面切换槽。
    btn_monitor_page = new QPushButton("监控");
    btn_playback_page = new QPushButton("回放");
    btn_setting_page = new QPushButton("设置");

    const QList<QPushButton *> nav_buttons = {
        btn_monitor_page, btn_playback_page, btn_setting_page
    };

    // QButtonGroup 只管理选中状态的互斥；真正的页面显示由 QStackedWidget 管理。
    QButtonGroup *nav_group = new QButtonGroup(this);
    nav_group->setExclusive(true);

    for (QPushButton *button : nav_buttons) {
        button->setObjectName("navButton");
        button->setCheckable(true);
        button->setMinimumSize(90, 46);
        nav_group->addButton(button);
        top_layout->addWidget(button);
    }
    btn_monitor_page->setChecked(true);

    // lab_time 会被 QTimer 每隔一秒更新，因此必须保存为成员变量。
    lab_time = new QLabel;
    lab_time->setObjectName("timeLabel");
    lab_time->setMinimumWidth(210);
    lab_time->setAlignment(Qt::AlignCenter);
    top_layout->addWidget(lab_time);

    QFrame *login_box = new QFrame;
    login_box->setObjectName("loginBox");
    login_box->setFixedWidth(88);
    QVBoxLayout *login_layout = new QVBoxLayout(login_box);
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

    // QStackedWidget 像一叠卡片：三个页面都会加入，但一次只显示其中一个。
    page_stack = new QStackedWidget;
    page_stack->setObjectName("pageStack");
    main_layout->addWidget(page_stack, 1);

    // ==================== 第 0 页：监控页面 ====================
    QFrame *monitor_page = new QFrame;
    monitor_page->setObjectName("contentArea");
    QHBoxLayout *content_layout = new QHBoxLayout(monitor_page);
    content_layout->setContentsMargins(24, 20, 24, 24);
    content_layout->setSpacing(20);

    QFrame *device_panel = new QFrame;
    device_panel->setObjectName("devicePanel");
    device_panel->setFixedWidth(245);
    QVBoxLayout *device_layout = new QVBoxLayout(device_panel);
    device_layout->setContentsMargins(20, 18, 20, 18);
    device_layout->setSpacing(12);

    QLabel *lab_devices = new QLabel("设备列表");
    lab_devices->setObjectName("sectionTitle");
    device_layout->addWidget(lab_devices);

    QFrame *line = new QFrame;
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
    content_layout->addWidget(device_panel);

    QFrame *monitor_panel = new QFrame;
    monitor_panel->setObjectName("monitorPanel");
    QVBoxLayout *monitor_layout = new QVBoxLayout(monitor_panel);
    monitor_layout->setContentsMargins(0, 0, 0, 12);
    monitor_layout->setSpacing(10);

    // QGridLayout 在监控页面内部负责单画面和 2×2 四画面的切换。
    QFrame *video_area = new QFrame;
    video_area->setObjectName("videoArea");
    video_layout = new QGridLayout(video_area);
    video_layout->setContentsMargins(0, 0, 0, 0);
    video_layout->setSpacing(3);

    for (int i = 0; i < 4; ++i) {
        video_windows[i] = new QLabel(
                    QStringLiteral("通道 %1：等待视频源").arg(i + 1));
        video_windows[i]->setObjectName("videoWindow");
        video_windows[i]->setAlignment(Qt::AlignCenter);
        video_windows[i]->setMinimumSize(180, 150);
        video_layout->addWidget(video_windows[i], i / 2, i % 2);
    }

    lab_video_hint = video_windows[0];
    monitor_layout->addWidget(video_area, 1);

    QHBoxLayout *control_layout = new QHBoxLayout;
    control_layout->setContentsMargins(18, 0, 18, 0);
    control_layout->setSpacing(12);

    btn_one = new QPushButton("单画面");
    btn_four = new QPushButton("四画面");
    btn_start_preview = new QPushButton("启动预览");
    btn_stop_preview = new QPushButton("停止预览");
    btn_start_preview->setObjectName("startPreviewButton");
    btn_stop_preview->setObjectName("stopPreviewButton");

    const QList<QPushButton *> control_buttons = {
        btn_one, btn_four, btn_start_preview, btn_stop_preview
    };
    for (QPushButton *button : control_buttons) {
        if (button->objectName().isEmpty()) {
            button->setObjectName("controlButton");
        }
        button->setMinimumHeight(38);
    }

    control_layout->addWidget(btn_one);
    control_layout->addWidget(btn_four);
    control_layout->addStretch();
    control_layout->addWidget(btn_start_preview);
    control_layout->addWidget(btn_stop_preview);

    monitor_layout->addLayout(control_layout);
    content_layout->addWidget(monitor_panel, 1);

    // ==================== 第 1 页：回放页面 ====================
    QFrame *playback_page = new QFrame;
    playback_page->setObjectName("playbackPage");
    QVBoxLayout *playback_layout = new QVBoxLayout(playback_page);
    playback_layout->setContentsMargins(28, 22, 28, 28);
    playback_layout->setSpacing(18);

    QLabel *playback_title = new QLabel("录像回放");
    playback_title->setObjectName("pageTitle");
    playback_layout->addWidget(playback_title);

    QHBoxLayout *playback_content = new QHBoxLayout;
    playback_content->setSpacing(20);

    // 左侧列表只显示数据库 video 表中的文件名。
    QFrame *record_panel = new QFrame;
    record_panel->setObjectName("pageCard");
    record_panel->setMinimumWidth(560);
    QVBoxLayout *record_layout = new QVBoxLayout(record_panel);

    QHBoxLayout *record_header_layout = new QHBoxLayout;
    QLabel *record_title = new QLabel("视频文件列表");
    record_title->setObjectName("sectionTitle");
    btn_refresh_records = new QPushButton("刷新记录");
    btn_refresh_records->setObjectName("videoRecordRefreshButton");
    btn_refresh_records->setMinimumHeight(34);
    record_header_layout->addWidget(record_title);
    record_header_layout->addStretch();
    record_header_layout->addWidget(btn_refresh_records);
    record_layout->addLayout(record_header_layout);

    video_record_list = new QListWidget;
    video_record_list->setObjectName("recordList");
    video_record_list->setAlternatingRowColors(true);
    record_layout->addWidget(video_record_list, 1);

    QHBoxLayout *page_layout = new QHBoxLayout;
    btn_previous_video_page = new QPushButton("上一页");
    btn_next_video_page = new QPushButton("下一页");
    lab_video_page = new QLabel("第 1 / 1 页，共 0 条");
    btn_previous_video_page->setObjectName("controlButton");
    btn_next_video_page->setObjectName("controlButton");
    lab_video_page->setObjectName("settingText");
    page_layout->addWidget(btn_previous_video_page);
    page_layout->addStretch();
    page_layout->addWidget(lab_video_page);
    page_layout->addStretch();
    page_layout->addWidget(btn_next_video_page);
    record_layout->addLayout(page_layout);
    playback_content->addWidget(record_panel, 2);

    QFrame *playback_panel = new QFrame;
    playback_panel->setObjectName("pageCard");
    QVBoxLayout *playback_view_layout = new QVBoxLayout(playback_panel);
    QLabel *playback_view = new QLabel("请选择左侧录像文件进行回放");
    playback_view->setObjectName("playbackWindow");
    playback_view->setAlignment(Qt::AlignCenter);
    playback_view_layout->addWidget(playback_view, 1);

    QHBoxLayout *playback_controls = new QHBoxLayout;
    QPushButton *playback_previous = new QPushButton("上一段");
    QPushButton *playback_play = new QPushButton("播放 / 暂停");
    QPushButton *playback_next = new QPushButton("下一段");
    const QList<QPushButton *> playback_buttons = {
        playback_previous, playback_play, playback_next
    };
    for (QPushButton *button : playback_buttons) {
        button->setObjectName("controlButton");
        button->setMinimumHeight(38);
        playback_controls->addWidget(button);
    }
    playback_view_layout->addLayout(playback_controls);
    playback_content->addWidget(playback_panel, 1);
    playback_layout->addLayout(playback_content, 1);

    // ==================== 第 2 页：设置页面 ====================
    QFrame *setting_page = new QFrame;
    setting_page->setObjectName("settingPage");
    QVBoxLayout *setting_layout = new QVBoxLayout(setting_page);
    setting_layout->setContentsMargins(28, 22, 28, 28);
    setting_layout->setSpacing(18);

    QLabel *setting_title = new QLabel("系统设置");
    setting_title->setObjectName("pageTitle");
    setting_layout->addWidget(setting_title);

    QFrame *setting_card = new QFrame;
    setting_card->setObjectName("pageCard");
    QVBoxLayout *setting_card_layout = new QVBoxLayout(setting_card);
    setting_card_layout->setContentsMargins(28, 24, 28, 24);
    setting_card_layout->setSpacing(18);

    QLabel *setting_hint = new QLabel(
        "配置四个通道的名称和视频来源。摄像头不能被多个通道重复占用。"
    );
    setting_hint->setObjectName("settingText");
    setting_hint->setWordWrap(true);
    setting_card_layout->addWidget(setting_hint);

    QGridLayout *config_grid = new QGridLayout;
    config_grid->setHorizontalSpacing(12);
    config_grid->setVerticalSpacing(10);
    const QStringList headers = {
        "通道", "名称", "来源", "摄像头", "本地视频", ""
    };
    for (int column = 0; column < headers.size(); ++column) {
        QLabel *header = new QLabel(headers.at(column));
        header->setObjectName("settingText");
        config_grid->addWidget(header, 0, column);
    }

    for (int i = 0; i < 4; ++i) {
        QLabel *channel_label = new QLabel(QString::number(i + 1));
        channel_label->setObjectName("settingText");

        channel_name_edits_[i] = new QLineEdit;
        channel_name_edits_[i]->setObjectName(
                    QStringLiteral("channelNameEdit_%1").arg(i + 1));

        channel_source_boxes_[i] = new QComboBox;
        channel_source_boxes_[i]->setObjectName(
                    QStringLiteral("channelSourceType_%1").arg(i + 1));
        channel_source_boxes_[i]->addItem(
                    "摄像头", static_cast<int>(ChannelSourceType::Camera));
        channel_source_boxes_[i]->addItem(
                    "本地视频", static_cast<int>(ChannelSourceType::File));

        channel_camera_boxes_[i] = new QComboBox;
        channel_camera_boxes_[i]->setObjectName(
                    QStringLiteral("channelCamera_%1").arg(i + 1));

        channel_file_edits_[i] = new QLineEdit;
        channel_file_edits_[i]->setObjectName(
                    QStringLiteral("channelFile_%1").arg(i + 1));
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

    btn_save_channel_configs = new QPushButton("保存通道配置");
    btn_save_channel_configs->setObjectName("saveChannelConfigButton");
    btn_save_channel_configs->setMinimumHeight(40);
    setting_card_layout->addWidget(btn_save_channel_configs,
                                   0, Qt::AlignRight);
    setting_card_layout->addStretch();
    setting_layout->addWidget(setting_card, 1);

    // addWidget() 的顺序决定索引：监控=0、回放=1、设置=2。
    page_stack->addWidget(monitor_page);
    page_stack->addWidget(playback_page);
    page_stack->addWidget(setting_page);
    page_stack->setCurrentIndex(0);
}

void MainWidget::init_style()
{
    // QSS 的 #名称会匹配控件的 objectName，例如 QLabel#timeLabel。
    setStyleSheet(
        "QWidget#mainWidget { background-color: #20242a; color: #d7d9dc; }"
        "QFrame#topBar { background-color: #252a31; border-bottom: 1px solid #3b414a; }"
        "QLabel#titleLabel { color: #d7d9dc; font: 30px 'Microsoft YaHei'; }"
        "QPushButton#navButton { color: #c7cbd1; background-color: #0f1115; "
        "border: 1px solid #343a43; border-radius: 8px; font: 16px 'Microsoft YaHei'; }"
        "QPushButton#navButton:hover { background-color: #353b45; }"
        "QPushButton#navButton:checked { color: white; border: 1px solid #3b82f6; "
        "background-color: #1f4f96; }"
        "QLabel#timeLabel { color: #d7d9dc; font: 18px Consolas; }"
        "QPushButton#loginButton { color: white; background-color: #2042a5; "
        "border: 2px solid #375bd0; border-radius: 33px; font: bold 15px 'Microsoft YaHei'; }"
        "QPushButton#loginButton:hover { background-color: #3158d5; }"
        "QLabel#loginLabel { color: #bfc3ca; font: 14px 'Microsoft YaHei'; }"
        "QStackedWidget#pageStack { background-color: #20242a; }"
        "QFrame#contentArea, QFrame#playbackPage, QFrame#settingPage { background-color: #20242a; }"
        "QFrame#devicePanel, QFrame#pageCard { background-color: #292e36; "
        "border: 1px solid #3a4049; border-radius: 6px; }"
        "QLabel#pageTitle { color: #f0f1f3; font: 26px 'Microsoft YaHei'; }"
        "QLabel#sectionTitle { color: #f0f1f3; font: 21px 'Microsoft YaHei'; }"
        "QLabel#settingText { color: #d5d8dd; font: 16px 'Microsoft YaHei'; }"
        "QFrame#separator { color: #777d86; background-color: #777d86; max-height: 1px; }"
        "QListWidget#deviceList, QListWidget#recordList { background: transparent; border: none; "
        "color: #cdd0d5; font: 16px 'Microsoft YaHei'; outline: none; }"
        "QListWidget#deviceList::item, QListWidget#recordList::item { height: 40px; padding-left: 8px; }"
        "QListWidget#deviceList::item:hover, QListWidget#recordList::item:hover { "
        "background-color: #353c46; border-radius: 4px; }"
        "QListWidget#deviceList::item:selected, QListWidget#recordList::item:selected { "
        "background-color: #3158a8; color: white; }"
        "QPushButton#videoRecordRefreshButton, QPushButton#refreshCamerasButton, "
        "QPushButton#saveChannelConfigButton { color: white; background-color: #3158a8; "
        "border: 1px solid #4775c5; border-radius: 5px; padding: 4px 14px; "
        "font: 14px 'Microsoft YaHei'; }"
        "QPushButton#videoRecordRefreshButton:hover, QPushButton#refreshCamerasButton:hover, "
        "QPushButton#saveChannelConfigButton:hover { background-color: #3b6bc3; }"
        "QTableView#videoRecordTable { background-color: #20252c; alternate-background-color: #292f38; "
        "color: #d7d9dc; gridline-color: #424a55; border: 1px solid #424a55; "
        "selection-background-color: #3158a8; selection-color: white; }"
        "QTableView#videoRecordTable QHeaderView::section { background-color: #343b45; "
        "color: #f0f1f3; border: none; border-right: 1px solid #4a535f; "
        "padding: 7px; font: 14px 'Microsoft YaHei'; }"
        "QFrame#monitorPanel { background-color: #292e36; border: 1px solid #3a4049; "
        "border-radius: 6px; }"
        "QFrame#videoArea { background-color: #11151a; }"
        "QLabel#videoWindow, QLabel#playbackWindow { background-color: #090b0e; color: #aeb3ba; "
        "font: 20px 'Microsoft YaHei'; border: 1px solid #343b45; }"
        "QLineEdit, QComboBox { color: #e1e4e8; background-color: #15191e; "
        "border: 1px solid #414852; border-radius: 4px; padding: 5px; }"
        "QPushButton#controlButton, QPushButton#startPreviewButton, "
        "QPushButton#stopPreviewButton { color: #d5d8dd; background-color: #15191e; "
        "border: 1px solid #414852; border-radius: 5px; padding: 5px 16px; "
        "font: 14px 'Microsoft YaHei'; }"
        "QPushButton#controlButton:hover, QPushButton#startPreviewButton:hover, "
        "QPushButton#stopPreviewButton:hover { color: white; background-color: #344052; }"
        "QPushButton#controlButton:checked { color: white; background-color: #3158a8; }"
    );
}

void MainWidget::init_connect()
{
    // 顶部导航：按钮只发出点击请求，三个槽决定显示 QStackedWidget 的哪个页面。
    connect(btn_monitor_page, SIGNAL(clicked()),
            this, SLOT(show_monitor_page()));
    connect(btn_playback_page, SIGNAL(clicked()),
            this, SLOT(show_playback_page()));
    connect(btn_setting_page, SIGNAL(clicked()),
            this, SLOT(show_setting_page()));

    connect(btn_login, SIGNAL(clicked()),
            this, SLOT(handle_login_button()));

    connect(btn_one, SIGNAL(clicked()),
            this, SLOT(show_single_window()));
    connect(btn_four, SIGNAL(clicked()),
            this, SLOT(show_multi_window()));

    connect(btn_refresh_records, SIGNAL(clicked()),
            this, SLOT(load_video_records()));
    connect(btn_previous_video_page, SIGNAL(clicked()),
            this, SLOT(load_previous_video_page()));
    connect(btn_next_video_page, SIGNAL(clicked()),
            this, SLOT(load_next_video_page()));

    connect(device_list, &QListWidget::itemClicked,
            this, &MainWidget::select_channel);
    connect(btn_start_preview, &QPushButton::clicked,
            this, &MainWidget::start_selected_preview);
    connect(btn_stop_preview, &QPushButton::clicked,
            this, &MainWidget::stop_selected_preview);
    connect(btn_refresh_cameras, &QPushButton::clicked,
            this, &MainWidget::refresh_camera_devices);
    connect(media_devices_, &QMediaDevices::videoInputsChanged,
            this, &MainWidget::refresh_camera_devices);
    connect(btn_save_channel_configs, &QPushButton::clicked,
            this, &MainWidget::save_channel_configs);

    for (int i = 0; i < 4; ++i) {
        connect(channel_source_boxes_[i],
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                [this, i](int) { update_source_controls(i); });
        connect(channel_file_buttons_[i], &QPushButton::clicked,
                this,
                [this, i]() { choose_channel_file(i); });
    }
}

void MainWidget::init_showtime()
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, SIGNAL(timeout()),
            this, SLOT(update_time()));
    m_timer->start();

    // 窗口刚显示时立即刷新，避免前一秒时间标签为空。
    update_time();
}

void MainWidget::show_monitor_page()
{
    // 索引 0 对应第一个 addWidget() 加入的监控页面。
    page_stack->setCurrentIndex(0);
}

void MainWidget::show_playback_page()
{
    if (!require_permission(AuthPermission::Playback, "查看录像回放")) {
        btn_monitor_page->setChecked(true);
        return;
    }
    page_stack->setCurrentIndex(1);
}

void MainWidget::show_setting_page()
{
    if (!require_permission(AuthPermission::Settings, "修改系统设置")) {
        btn_monitor_page->setChecked(true);
        return;
    }
    page_stack->setCurrentIndex(2);
}

void MainWidget::handle_login_button()
{
    if (auth_session.isLoggedIn()) {
        logout();
        return;
    }
    emit login_requested();
}

void MainWidget::setLoggedInUser(const QString &userName)
{
    auth_session.login(userName);
    lab_login->setText(userName);
    btn_login->setText("退出");

    // 登录通过后从第一页加载视频文件。
    current_video_page = 1;
    load_video_records();
}

void MainWidget::load_video_records()
{
    load_video_page(current_video_page);
}

bool MainWidget::load_video_page(int requestedPage)
{
    if (!require_permission(AuthPermission::Playback, "查询录像记录")) {
        return false;
    }

    VideoPage page;
    QString error_text;
    if (!video_service.loadPage(requestedPage, page, error_text)) {
        QMessageBox::warning(this, "视频查询失败", error_text);
        return false;
    }

    // 只有查询成功后才提交页码，避免失败时状态与画面不一致。
    current_video_page = page.currentPage;
    video_record_list->clear();
    video_record_list->addItems(page.fileNames);
    lab_video_page->setText(
        QString("第 %1 / %2 页，共 %3 条")
            .arg(page.currentPage)
            .arg(page.totalPages)
            .arg(page.totalRecords)
    );
    btn_previous_video_page->setEnabled(page.currentPage > 1);
    btn_next_video_page->setEnabled(page.currentPage < page.totalPages);
    return true;
}

bool MainWidget::require_permission(AuthPermission permission,
                                    const QString &actionName)
{
    if (auth_session.canAccess(permission)) {
        return true;
    }

    QMessageBox::information(
        this,
        "需要登录",
        QString("请先登录，再%1。未登录状态仅允许查看实时画面。")
            .arg(actionName)
    );
    return false;
}

void MainWidget::logout()
{
    auth_session.logout();
    current_video_page = 1;
    video_record_list->clear();
    lab_video_page->setText("第 1 / 1 页，共 0 条");
    lab_login->setText("未登录");
    btn_login->setText("登录");
    btn_monitor_page->setChecked(true);
    page_stack->setCurrentIndex(0);
    QMessageBox::information(this, "退出登录", "已退出，当前为访客模式。");
}

void MainWidget::load_previous_video_page()
{
    load_video_page(current_video_page - 1);
}

void MainWidget::load_next_video_page()
{
    load_video_page(current_video_page + 1);
}

void MainWidget::update_time()
{
    lab_time->setText(
        QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")
    );
}

void MainWidget::show_single_window()
{
    for (int i = 0; i < 4; ++i) {
        video_layout->removeWidget(video_windows[i]);
        video_windows[i]->hide();
    }

    const int index = selected_channel_ - 1;
    video_layout->addWidget(video_windows[index], 0, 0);
    video_windows[index]->show();
}

void MainWidget::show_multi_window()
{
    // i / 2 是行号，i % 2 是列号，形成 2×2 网格。
    for (int i = 0; i < 4; ++i) {
        video_layout->removeWidget(video_windows[i]);
        video_layout->addWidget(video_windows[i], i / 2, i % 2);
        video_windows[i]->show();
    }
}

void MainWidget::init_workers()
{
    qRegisterMetaType<cv::Mat>("cv::Mat");
    for (int i = 0; i < 4; ++i) {
        monitor_threads_[i] = new QThread(this);
        monitor_tasks_[i] = new MonitorTask;
        monitor_tasks_[i]->moveToThread(monitor_threads_[i]);

        connect(monitor_threads_[i], &QThread::finished,
                monitor_tasks_[i], &QObject::deleteLater);
        connect(monitor_tasks_[i], &MonitorTask::frameReady,
                this, &MainWidget::display_frame, Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::captureStateChanged,
                this, &MainWidget::on_capture_state_changed,
                Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::errorOccurred,
                this, &MainWidget::show_capture_error,
                Qt::QueuedConnection);

        monitor_threads_[i]->start();
    }
}

void MainWidget::shutdown_workers()
{
    for (int i = 0; i < 4; ++i) {
        if (monitor_tasks_[i] && monitor_threads_[i]
                && monitor_threads_[i]->isRunning()) {
            QMetaObject::invokeMethod(monitor_tasks_[i], "stopCapture",
                                      Qt::BlockingQueuedConnection);
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (monitor_threads_[i] && monitor_threads_[i]->isRunning()) {
            monitor_threads_[i]->quit();
            monitor_threads_[i]->wait();
        }
    }
}

void MainWidget::load_channel_configs()
{
    QString errorText;
    if (!channel_config_service_.load(channel_configs_, errorText)) {
        channel_configs_ = ChannelConfigPolicy::defaults();
        video_windows[0]->setText(
            QStringLiteral("配置加载失败，已使用默认值：%1").arg(errorText));
    }
}

void MainWidget::apply_channel_configs_to_ui()
{
    for (int i = 0; i < 4; ++i) {
        const ChannelConfig &config = channel_configs_.at(i);
        channel_name_edits_[i]->setText(config.name);
        channel_source_boxes_[i]->setCurrentIndex(
                    config.sourceType == ChannelSourceType::Camera ? 0 : 1);

        int cameraRow = channel_camera_boxes_[i]->findData(config.cameraIndex);
        if (cameraRow < 0) {
            channel_camera_boxes_[i]->addItem(
                QStringLiteral("摄像头 %1（当前不可用）")
                    .arg(config.cameraIndex),
                config.cameraIndex);
            cameraRow = channel_camera_boxes_[i]->count() - 1;
        }
        channel_camera_boxes_[i]->setCurrentIndex(cameraRow);
        channel_file_edits_[i]->setText(config.filePath);
        update_source_controls(i);
    }
    refresh_channel_list();
}

QList<ChannelConfig> MainWidget::collect_channel_configs_from_ui() const
{
    QList<ChannelConfig> configs;
    for (int i = 0; i < 4; ++i) {
        ChannelConfig config;
        config.channelId = i + 1;
        config.name = channel_name_edits_[i]->text().trimmed();
        config.sourceType = static_cast<ChannelSourceType>(
                    channel_source_boxes_[i]->currentData().toInt());
        config.cameraIndex = channel_camera_boxes_[i]->currentData().toInt();
        config.filePath = channel_file_edits_[i]->text().trimmed();
        configs.append(config);
    }
    return configs;
}

void MainWidget::refresh_channel_list()
{
    const int previousRow = device_list->currentRow();
    device_list->clear();
    for (int i = 0; i < channel_configs_.size(); ++i) {
        const ChannelConfig &config = channel_configs_.at(i);
        const QString source = config.sourceType == ChannelSourceType::Camera
                ? QStringLiteral("摄像头 %1").arg(config.cameraIndex)
                : (config.filePath.isEmpty()
                   ? QStringLiteral("未选择视频")
                   : QStringLiteral("本地视频"));
        const QString state = preview_running_[i]
                ? QStringLiteral(" · 运行中")
                : QString();
        device_list->addItem(
            QStringLiteral("%1 · %2%3").arg(config.name, source, state));
    }
    const int row = previousRow >= 0 ? previousRow : selected_channel_ - 1;
    device_list->setCurrentRow(qBound(0, row, 3));
}

void MainWidget::update_source_controls(int index)
{
    if (index < 0 || index >= 4) {
        return;
    }
    const bool cameraSelected =
            channel_source_boxes_[index]->currentData().toInt()
            == static_cast<int>(ChannelSourceType::Camera);
    channel_camera_boxes_[index]->setEnabled(cameraSelected);
    channel_file_edits_[index]->setEnabled(!cameraSelected);
    channel_file_buttons_[index]->setEnabled(!cameraSelected);
}

void MainWidget::choose_channel_file(int index)
{
    if (index < 0 || index >= 4) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("为通道 %1 选择视频文件").arg(index + 1),
        channel_file_edits_[index]->text(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mkv *.mov);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        channel_file_edits_[index]->setText(path);
    }
}

void MainWidget::refresh_camera_devices()
{
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (int i = 0; i < 4; ++i) {
        const int previousIndex = channel_camera_boxes_[i]->currentData().toInt();
        channel_camera_boxes_[i]->clear();
        for (int cameraIndex = 0; cameraIndex < cameras.size(); ++cameraIndex) {
            channel_camera_boxes_[i]->addItem(
                QStringLiteral("%1（索引 %2）")
                    .arg(cameras.at(cameraIndex).description())
                    .arg(cameraIndex),
                cameraIndex);
        }
        if (cameras.isEmpty()) {
            channel_camera_boxes_[i]->addItem(
                        QStringLiteral("未检测到摄像头"), -1);
        }
        const int restoredRow = channel_camera_boxes_[i]->findData(previousIndex);
        if (restoredRow >= 0) {
            channel_camera_boxes_[i]->setCurrentIndex(restoredRow);
        }
    }
}

void MainWidget::save_channel_configs()
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

    for (int i = 0; i < 4; ++i) {
        stop_channel(i);
    }
    channel_configs_ = configs;
    refresh_channel_list();
    start_configured_previews();
    QMessageBox::information(this, "保存成功", "四通道名称和来源已保存并应用。");
}

void MainWidget::select_channel(QListWidgetItem *item)
{
    const int row = device_list->row(item);
    if (row < 0 || row >= 4) {
        return;
    }
    selected_channel_ = row + 1;
    lab_video_hint = video_windows[row];
}

void MainWidget::start_selected_preview()
{
    start_channel(selected_channel_ - 1);
}

void MainWidget::stop_selected_preview()
{
    stop_channel(selected_channel_ - 1);
}

void MainWidget::start_configured_previews()
{
    for (int i = 0; i < channel_configs_.size(); ++i) {
        QString errorText;
        if (ChannelConfigPolicy::validateForStart(channel_configs_.at(i),
                                                  errorText)) {
            start_channel(i);
        } else {
            video_windows[i]->setText(
                QStringLiteral("%1\n%2")
                    .arg(channel_configs_.at(i).name, errorText));
        }
    }
}

void MainWidget::start_channel(int index)
{
    if (index < 0 || index >= channel_configs_.size()) {
        return;
    }
    QString errorText;
    const ChannelConfig config = channel_configs_.at(index);
    if (!ChannelConfigPolicy::validateForStart(config, errorText)) {
        QMessageBox::information(this, "无法启动预览", errorText);
        return;
    }

    video_windows[index]->setText(
                QStringLiteral("%1\n正在连接...").arg(config.name));
    MonitorTask *task = monitor_tasks_[index];
    QMetaObject::invokeMethod(
        task,
        [task, config]() { task->startCapture(config); },
        Qt::QueuedConnection);
}

void MainWidget::stop_channel(int index)
{
    if (index < 0 || index >= 4 || monitor_tasks_[index] == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(monitor_tasks_[index], "stopCapture",
                              Qt::QueuedConnection);
}

void MainWidget::display_frame(const cv::Mat &frame, int channelId)
{
    if (channelId < 1 || channelId > 4 || frame.empty()) {
        return;
    }

    cv::Mat rgb;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    } else if (frame.channels() == 4) {
        cv::cvtColor(frame, rgb, cv::COLOR_BGRA2RGBA);
    } else {
        cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
    }

    const QImage::Format format = rgb.channels() == 4
            ? QImage::Format_RGBA8888
            : QImage::Format_RGB888;
    const QImage image(rgb.data, rgb.cols, rgb.rows,
                       static_cast<int>(rgb.step), format);
    QLabel *target = video_windows[channelId - 1];
    target->setPixmap(QPixmap::fromImage(image.copy()).scaled(
        target->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWidget::show_capture_error(int channelId, const QString &message)
{
    if (channelId < 1 || channelId > 4) {
        return;
    }
    preview_running_[channelId - 1] = false;
    video_windows[channelId - 1]->setText(
        QStringLiteral("%1\n%2")
            .arg(channel_configs_.at(channelId - 1).name, message));
    refresh_channel_list();
}

void MainWidget::on_capture_state_changed(int channelId, bool running)
{
    if (channelId < 1 || channelId > 4) {
        return;
    }
    preview_running_[channelId - 1] = running;
    if (!running) {
        video_windows[channelId - 1]->setText(
            QStringLiteral("%1\n预览已停止")
                .arg(channel_configs_.at(channelId - 1).name));
    }
    refresh_channel_list();
}
