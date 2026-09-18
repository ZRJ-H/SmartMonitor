#include "mainwidget.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      btn_monitor_page(nullptr),
      btn_playback_page(nullptr),
      btn_setting_page(nullptr),
      btn_login(nullptr),
      btn_one(nullptr),
      btn_four(nullptr),
      btn_refresh_records(nullptr),
      lab_login(nullptr),
      lab_time(nullptr),
      lab_video_hint(nullptr),
      page_stack(nullptr),
      video_record_list(nullptr),
      lab_video_page(nullptr),
      btn_previous_video_page(nullptr),
      btn_next_video_page(nullptr),
      video_service(5),
      current_video_page(1),
      logged_in(false),
      video_layout(nullptr),
      m_timer(nullptr)
{
    // 数组中的控件会在 init_ui() 中创建，这里先初始化为空指针。
    for (int i = 0; i < 4; ++i) {
        video_windows[i] = nullptr;
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
}

MainWidget::~MainWidget()
{
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

    QListWidget *device_list = new QListWidget;
    device_list->setObjectName("deviceList");
    device_list->addItem("东门摄像头");
    device_list->addItem("西门摄像头");
    device_list->addItem("停车场摄像头");
    device_list->addItem("机房摄像头");
    device_layout->addWidget(device_list);
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

    const QStringList window_names = {
        "请先点击右上角登录",
        "监控窗口 2",
        "监控窗口 3",
        "监控窗口 4"
    };

    for (int i = 0; i < 4; ++i) {
        video_windows[i] = new QLabel(window_names.at(i));
        video_windows[i]->setObjectName("videoWindow");
        video_windows[i]->setAlignment(Qt::AlignCenter);
        video_windows[i]->setMinimumSize(180, 150);
    }

    // 默认使用单画面模式。
    lab_video_hint = video_windows[0];
    video_layout->addWidget(video_windows[0], 0, 0);
    for (int i = 1; i < 4; ++i) {
        video_windows[i]->hide();
    }
    monitor_layout->addWidget(video_area, 1);

    QHBoxLayout *control_layout = new QHBoxLayout;
    control_layout->setContentsMargins(18, 0, 18, 0);
    control_layout->setSpacing(12);

    btn_one = new QPushButton("单画面");
    btn_four = new QPushButton("四画面");
    QPushButton *btn_previous = new QPushButton("上一段");
    QPushButton *btn_play = new QPushButton("播放");
    QPushButton *btn_next = new QPushButton("下一段");

    const QList<QPushButton *> control_buttons = {
        btn_one, btn_four, btn_previous, btn_play, btn_next
    };
    for (QPushButton *button : control_buttons) {
        button->setObjectName("controlButton");
        button->setMinimumHeight(38);
    }

    control_layout->addWidget(btn_one);
    control_layout->addWidget(btn_four);
    control_layout->addStretch();
    control_layout->addWidget(btn_previous);
    control_layout->addWidget(btn_play);
    control_layout->addWidget(btn_next);
    control_layout->addStretch();

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
        "当前页面用于演示 QStackedWidget 页面切换，设置项暂不保存到文件。"
    );
    setting_hint->setObjectName("settingText");
    setting_hint->setWordWrap(true);
    setting_card_layout->addWidget(setting_hint);

    const QStringList setting_names = {
        "开机自动进入监控页面",
        "登录后显示设备列表",
        "录像文件保存 30 天"
    };
    for (const QString &name : setting_names) {
        QHBoxLayout *row = new QHBoxLayout;
        QLabel *label = new QLabel(name);
        label->setObjectName("settingText");
        QPushButton *button = new QPushButton("已启用");
        button->setObjectName("controlButton");
        button->setCheckable(true);
        button->setChecked(true);
        row->addWidget(label);
        row->addStretch();
        row->addWidget(button);
        setting_card_layout->addLayout(row);
    }
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
        "QPushButton#videoRecordRefreshButton { color: white; background-color: #3158a8; "
        "border: 1px solid #4775c5; border-radius: 5px; padding: 4px 14px; "
        "font: 14px 'Microsoft YaHei'; }"
        "QPushButton#videoRecordRefreshButton:hover { background-color: #3b6bc3; }"
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
        "QPushButton#controlButton { color: #d5d8dd; background-color: #15191e; "
        "border: 1px solid #414852; border-radius: 5px; padding: 5px 16px; "
        "font: 14px 'Microsoft YaHei'; }"
        "QPushButton#controlButton:hover { color: white; background-color: #344052; }"
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
            this, SLOT(to_login_win()));

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
    // 索引 1 对应回放页面。
    page_stack->setCurrentIndex(1);
}

void MainWidget::show_setting_page()
{
    // 索引 2 对应设置页面。
    page_stack->setCurrentIndex(2);
}

void MainWidget::to_login_win()
{
    emit login_requested();
}

void MainWidget::setLoggedInUser(const QString &userName)
{
    logged_in = true;
    lab_login->setText(userName);
    btn_login->setText("用户");
    lab_video_hint->setText("登录成功，请从左侧选择设备开始预览");

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
    if (!logged_in) {
        QMessageBox::information(this, "提示", "请先登录，再查询录像记录。");
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
    // removeWidget() 只从布局中移除控件，不会删除控件对象。
    for (int i = 0; i < 4; ++i) {
        video_layout->removeWidget(video_windows[i]);
        video_windows[i]->hide();
    }

    video_layout->addWidget(video_windows[0], 0, 0);
    video_windows[0]->show();
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
