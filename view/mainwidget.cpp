// 【阅读导引】
// 主界面协调：连接登录、通道、录像、图片、日志、恢复和备份；控件只能在 GUI 线程操作。
// 先读 init_workers/start_channel，再沿帧信号读采集与录像，最后读封口后的保存链。
// 当前预览和录像都经过 LatestFrameBuffer；慢消费会覆盖旧帧，不保证逐帧无损录制。

// 实现主窗口布局、权限控制、录像分页、通道配置以及四路视频采集线程与画面显示。
#include "mainwidget.h"
#include "apptheme.h"
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QSettings>
#include "../service/backupservice.h"
#include "../service/channelconfigpolicy.h"
#include "../service/monitortask.h"
#include "../service/recordtask.h"
#include "videolabel.h"
#include "imagepage.h"
#include "operationlogpage.h"
#include "../service/systemconfigservice.h"
#include "../service/imageservice.h"
#include "../service/eventservice.h"
#include "../service/mediarecoveryservice.h"
#include "../service/operationlogservice.h"
#include "../util/dbconn.h"
#include <QSpinBox>
#include <QVideoSink>
#include <QVideoFrame>


#include <QButtonGroup>
#include <QCameraDevice>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QUrl>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStorageInfo>
#include <QScrollArea>
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

// 包装不可变 OpenCV 像素；调用方保留 Mat 引用直到 QImage 消费结束。
namespace {
QImage frameImage(const cv::Mat &frame)
{
    if (frame.empty() || frame.depth() != CV_8U) return {};
    QImage::Format format;
    switch (frame.channels()) {
    case 1: format = QImage::Format_Grayscale8; break;
    case 3: format = QImage::Format_BGR888; break;
    case 4: format = QImage::Format_ARGB32; break; // Windows BGRA 字节顺序
    default: return {};
    }
    // QImage/QPixmap 在同尺寸时可能共享像素；让最后一个图像引用负责释放 Mat。
    // 使用 const 数据构造也保证 Qt 如需写入会自行分离。
    auto *pixels = new cv::Mat(frame);
    return QImage(static_cast<const uchar*>(pixels->data), pixels->cols, pixels->rows,
                  static_cast<qsizetype>(pixels->step), format,
                  [](void *owner) { delete static_cast<cv::Mat*>(owner); }, pixels);
}
}


// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent),
      btn_monitor_page(nullptr),
      btn_playback_page(nullptr),
      btn_setting_page(nullptr),
      btn_login(nullptr),
      btn_one(nullptr),
      btn_four(nullptr),
      btn_refresh_records(nullptr),
      btn_refresh_cameras(nullptr),
      btn_save_channel_configs(nullptr),
      btn_play_record(nullptr),
      btn_pause_playback(nullptr),
      btn_resume_playback(nullptr),
      btn_stop_playback(nullptr),
      btn_playback_snapshot(nullptr),
      btn_save_recording_settings(nullptr),
      btn_save_detection_settings(nullptr),
      lab_login(nullptr),
      lab_time(nullptr),
      lab_video_hint(nullptr),
      lab_storage_warning(nullptr),
      lab_playback_status(nullptr),
      page_stack(nullptr),
      device_list(nullptr),
      video_record_list(nullptr),
      lab_video_page(nullptr),
      btn_previous_video_page(nullptr),
      btn_next_video_page(nullptr),
      query_start_time_(nullptr),
      query_end_time_(nullptr),
      query_channel_(nullptr),
      playback_video_(nullptr),
      playback_player_(new QMediaPlayer(this)),
      playback_audio_(new QAudioOutput(this)),
      playback_slider_(nullptr),
      playback_speed_(nullptr),
      playback_duration_(0),
      video_service(5),
      current_video_page(1),
      auth_session(),
      video_layout(nullptr),
      m_timer(nullptr),
      media_devices_(new QMediaDevices(this)),
      record_directory_edit_(nullptr),
      record_segment_seconds_(nullptr),
      detection_enabled_box_(nullptr),
      yolo_model_edit_(nullptr),
      detection_confidence_spin_(nullptr),
      record_directory_(QDir(QCoreApplication::applicationDirPath()).filePath("records")),
      record_segment_seconds_value_(10),
      detection_enabled_value_(false),
      detection_confidence_value_(0.5),
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
        record_threads_[i] = nullptr;
        record_tasks_[i] = nullptr;
        pending_event_confidences_[i] = 0.0;
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
    load_system_settings();
    load_channel_configs();
    refresh_camera_devices();
    apply_channel_configs_to_ui();
    init_management_pages();
    for (auto window : video_windows) window->setText("请先登录");
    // 监控随进程启动，不能以是否有人登录决定是否留存录像。
    init_workers();
    if (qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") != "0")
        QTimer::singleShot(0, this, &MainWidget::start_configured_previews);
}

// 对象销毁时释放本类负责的资源；依赖的外部对象需按约定保持有效。
MainWidget::~MainWidget()
{
    preview_delivery_enabled_.store(false);
    auth_session.logout();
    shutdown_workers();
}

// 创建控件和布局并保存成员指针；业务信号连接主要在 init_connect 中建立。
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
    images_button_ = new QPushButton("图片");
    logs_button_ = new QPushButton("日志");
    images_button_->setObjectName("imagesNavigation");
    logs_button_->setObjectName("logsNavigation");
    for(auto button : {images_button_,logs_button_}) {
        button->setMinimumSize(65,46);top_layout->addWidget(button);
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

    // QStackedWidget 像一叠卡片：各业务页面依次加入，一次只显示其中一个。
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
    device_panel->setMinimumWidth(190);
    device_panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
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
    content_layout->addWidget(device_panel, 1);

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
    video_layout->setRowStretch(0, 1);
    video_layout->setRowStretch(1, 1);
    video_layout->setColumnStretch(0, 1);
    video_layout->setColumnStretch(1, 1);

    for (int i = 0; i < 4; ++i) {
        video_windows[i] = new VideoLabel(
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
    detection_enabled_box_ = new QCheckBox("全局人形检测");
    detection_enabled_box_->setObjectName("globalDetectionSwitch");
    detection_enabled_box_->setToolTip("统一控制四路 YOLO 人形检测；模型和置信度在设置页配置。");
    const QList<QPushButton *> control_buttons = {
        btn_one, btn_four
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
    control_layout->addWidget(detection_enabled_box_);

    monitor_layout->addLayout(control_layout);
    lab_storage_warning = new QLabel;
    lab_storage_warning->setStyleSheet("color:#ffb74d;font-weight:bold;");
    lab_storage_warning->setWordWrap(true);
    monitor_layout->addWidget(lab_storage_warning);
    content_layout->addWidget(monitor_panel, 3);

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
    record_panel->setObjectName("recordFilterPanel");
    record_panel->setMinimumWidth(250);
    record_panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
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

    QGridLayout *filter_layout = new QGridLayout;
    filter_layout->setObjectName("playbackFilterGrid");
    query_start_time_ = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1));
    query_end_time_ = new QDateTimeEdit(QDateTime::currentDateTime());
    query_start_time_->setCalendarPopup(true);
    query_end_time_->setCalendarPopup(true);
    query_start_time_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    query_end_time_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    query_channel_ = new QComboBox;
    query_channel_->addItem("全部通道", 0);
    for (int channel = 1; channel <= 4; ++channel)
        query_channel_->addItem(QStringLiteral("通道 %1").arg(channel), channel);
    query_type_=new QComboBox;
    query_type_->addItem("全部录像",-1);query_type_->addItem("普通录像",0);query_type_->addItem("事件录像",1);
    query_start_time_->setObjectName("playbackStartTime");
    query_end_time_->setObjectName("playbackEndTime");
    query_start_time_->setToolTip("开始时间（年-月-日 时:分:秒）");
    query_end_time_->setToolTip("结束时间（年-月-日 时:分:秒）");
    // 时间与类别分成两行，忽略默认宽度提示，防止日期编辑器撑宽左栏。
    for (auto date : {query_start_time_, query_end_time_}) {
        date->setDisplayFormat("yy-MM-dd HH:mm:ss");
        date->setMinimumWidth(0);
        date->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
    for (auto combo : {query_channel_, query_type_}) {
        combo->setMinimumWidth(0);
        combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
    filter_layout->addWidget(query_start_time_,0,0);
    filter_layout->addWidget(query_channel_,0,1);
    filter_layout->addWidget(query_end_time_,1,0);
    filter_layout->addWidget(query_type_,1,1);
    filter_layout->setColumnStretch(0,2);
    filter_layout->setColumnStretch(1,1);
    record_layout->addLayout(filter_layout);

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
    playback_content->addWidget(record_panel, 3);

    QFrame *playback_panel = new QFrame;
    playback_panel->setObjectName("playbackDisplayPanel");
    QVBoxLayout *playback_view_layout = new QVBoxLayout(playback_panel);
    playback_video_ = new QVideoWidget;
    playback_video_->setObjectName("playbackWindow");
    playback_video_->setMinimumSize(360, 260);
    playback_player_->setAudioOutput(playback_audio_);
    playback_player_->setVideoOutput(playback_video_);
    playback_audio_->setVolume(0.7);
    playback_view_layout->addWidget(playback_video_, 1);

    lab_playback_status = new QLabel("请选择左侧录像文件进行回放");
    lab_playback_status->setObjectName("settingText");
    playback_view_layout->addWidget(lab_playback_status);
    playback_slider_ = new QSlider(Qt::Horizontal);
    playback_slider_->setRange(0, 1000);
    playback_view_layout->addWidget(playback_slider_);

    QHBoxLayout *playback_controls = new QHBoxLayout;
    btn_play_record = new QPushButton("播放");
    btn_pause_playback = new QPushButton("暂停");
    btn_resume_playback = new QPushButton("继续");
    btn_stop_playback = new QPushButton("停止");
    btn_playback_snapshot = new QPushButton("回放截图");
    playback_speed_ = new QComboBox;
    playback_speed_->addItem("1×", 1.0);
    playback_speed_->addItem("2×", 2.0);
    playback_speed_->addItem("4×", 4.0);
    const QList<QPushButton *> playback_buttons = {
        btn_play_record, btn_pause_playback, btn_resume_playback,
        btn_stop_playback, btn_playback_snapshot
    };
    for (QPushButton *button : playback_buttons) {
        button->setObjectName("controlButton");
        button->setMinimumHeight(38);
        playback_controls->addWidget(button);
    }
    playback_controls->addWidget(playback_speed_);
    playback_view_layout->addLayout(playback_controls);
    playback_content->addWidget(playback_panel, 7);
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
    setting_card_layout->setSizeConstraint(QLayout::SetMinimumSize);

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

    QGridLayout *recording_grid = new QGridLayout;
    QLabel *recording_title = new QLabel("录像与智能检测");
    recording_title->setObjectName("sectionTitle");
    setting_card_layout->addWidget(recording_title);
    record_directory_edit_ = new QLineEdit;
    QPushButton *browse_record_directory = new QPushButton("浏览目录");
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
    QPushButton *browse_yolo_model = new QPushButton("浏览模型");
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
    image_directory_edit_=new QLineEdit;image_directory_edit_->setObjectName("imageDirectoryEdit");
    recording_grid->addWidget(new QLabel("图片目录"),5,0);recording_grid->addWidget(image_directory_edit_,5,1,1,2);
    recording_grid->addWidget(new QLabel("采集节奏"),6,0);recording_grid->addWidget(new QLabel("文件按源帧率播放；摄像头按设备输出采集"),6,1,1,2);

    setting_card_layout->addLayout(recording_grid);
    QHBoxLayout *feature_save_layout = new QHBoxLayout;
    btn_save_recording_settings = new QPushButton("保存录像设置");
    btn_save_detection_settings = new QPushButton("保存检测设置");
    btn_save_recording_settings->setObjectName("saveRecordingSettingsButton");
    btn_save_detection_settings->setObjectName("saveDetectionSettingsButton");
    feature_save_layout->addWidget(btn_save_recording_settings);
    feature_save_layout->addWidget(btn_save_detection_settings);
    feature_save_layout->addStretch();
    setting_card_layout->addLayout(feature_save_layout);
    connect(browse_record_directory, &QPushButton::clicked,
            this, &MainWidget::browse_record_directory);
    connect(browse_yolo_model, &QPushButton::clicked,
            this, &MainWidget::browse_yolo_model);

    btn_save_channel_configs = new QPushButton("保存通道配置");
    btn_save_channel_configs->setObjectName("saveChannelConfigButton");
    btn_save_channel_configs->setMinimumHeight(40);
    setting_card_layout->addWidget(btn_save_channel_configs,
                                   0, Qt::AlignRight);
    setting_card_layout->addStretch();
    // 设置项较多，小窗口应滚动而不是把输入框和文字压扁。
    auto setting_scroll = new QScrollArea;
    setting_scroll->setObjectName("settingsScrollArea");
    setting_scroll->setWidgetResizable(true);
    setting_scroll->setFrameShape(QFrame::NoFrame);
    setting_scroll->setWidget(setting_card);
    setting_layout->addWidget(setting_scroll, 1);

    // addWidget() 的顺序决定索引：监控=0、回放=1、设置=2。
    page_stack->addWidget(monitor_page);
    page_stack->addWidget(playback_page);
    page_stack->addWidget(setting_page);
    page_stack->setCurrentIndex(0);
}

// 应用统一主题；objectName 同时供 QSS 和 UI 测试定位控件使用。
void MainWidget::init_style()
{
    AppTheme::applyMain(this);
}

// 把按钮、选择变化与播放器状态连接到槽；这里只建立关系，不立即执行用户操作。
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
    connect(video_record_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item, QListWidgetItem *) {
        if (item && item->data(Qt::UserRole+2).toBool()) play_selected_record();
    });
    connect(video_record_list, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *item) { if (!item->data(Qt::UserRole+2).toBool()) play_selected_record(); });
    connect(btn_play_record, &QPushButton::clicked,
            this, &MainWidget::play_selected_record);
    connect(btn_pause_playback, &QPushButton::clicked,
            this, &MainWidget::pause_playback);
    connect(btn_resume_playback, &QPushButton::clicked,
            this, &MainWidget::resume_playback);
    connect(btn_stop_playback, &QPushButton::clicked,
            this, &MainWidget::stop_playback);
    connect(btn_playback_snapshot, &QPushButton::clicked,
            this, &MainWidget::take_playback_snapshot);
    connect(playback_slider_, &QSlider::sliderReleased, this,
            [this]() { seek_playback(playback_slider_->value()); });
    connect(playback_speed_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWidget::change_playback_speed);
    connect(playback_player_, &QMediaPlayer::positionChanged,
            this, &MainWidget::update_playback_position);
    connect(playback_player_, &QMediaPlayer::durationChanged,
            this, &MainWidget::update_playback_duration);
    connect(btn_save_recording_settings, &QPushButton::clicked,
            this, &MainWidget::save_recording_settings);
    connect(btn_save_detection_settings, &QPushButton::clicked,
            this, &MainWidget::save_detection_settings);

    connect(device_list, &QListWidget::currentItemChanged,
            this, &MainWidget::select_channel);
    connect(detection_enabled_box_, &QCheckBox::clicked, this, [this](bool enabled) {
        const auto restore = [this] {
            const QSignalBlocker blocker(detection_enabled_box_);
            detection_enabled_box_->setChecked(detection_allowed_ && detection_enabled_value_);
        };
        if (!auth_session.isLoggedIn() || !detection_allowed_) { restore(); return; }
        if (enabled && !QFileInfo::exists(yolo_model_path_)) {
            restore();
            lab_storage_warning->setText("启用检测前，请在设置页选择有效的 ONNX 模型。");
            return;
        }
        if (!config_model_.upsertValue("detection.enabled", enabled ? "1" : "0")) {
            restore();
            lab_storage_warning->setText("检测开关保存失败，请检查数据库连接。");
            return;
        }
        detection_enabled_value_ = enabled;
        apply_detection_mode();
        lab_storage_warning->setText(enabled ? "已请求开启四路人形检测" : "四路人形检测已关闭");
        audit("修改配置", enabled ? "开启全局人形检测" : "关闭全局人形检测");
    });
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

// 创建 GUI 定时器更新顶部时钟，与采集线程的定时器职责不同。
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

// 保存真实用户 ID，更新管理入口并恢复登录后的预览显示。
void MainWidget::setLoggedInUser(int userId, const QString &userName)
{
    if (userId < 1 || userName.trimmed().isEmpty()) return;
    auth_session.login(userId, userName);
    preview_delivery_enabled_.store(true);
    update_management_access();
    audit("登录","管理员登录成功");
    lab_login->setText(userName);
    btn_login->setText("退出");
    for (int i = 0; i < 4; ++i) {
        video_windows[i]->setText(QStringLiteral("%1\n%2").arg(channel_configs_.at(i).name,
            preview_running_[i] ? "正在接收画面" : "通道未启动"));
    }

    // 登录通过后从第一页加载视频文件。
    current_video_page = 1;
    load_video_records();
}

// 按当前筛选重新加载录像列表，交由分页服务校验条件。
void MainWidget::load_video_records()
{
    current_video_page = 1;
    load_video_page(current_video_page);
}

// 把控件值整理成 VideoQuery；查询条件对象本身不执行 SQL。
VideoQuery MainWidget::current_video_query() const
{
    VideoQuery query;
    query.startTime = query_start_time_->dateTime();
    query.endTime = query_end_time_->dateTime();
    query.channelId = query_channel_->currentData().toInt();
    query.eventType=query_type_->currentData().toInt();
    return query;
}

// 加载成功才更新列表和页码；每个条目携带录像 ID，回放不靠猜文件名。
bool MainWidget::load_video_page(int requestedPage)
{
    if (!require_permission(AuthPermission::Playback, "查询录像记录")) {
        return false;
    }

    VideoPage page;
    QString error_text;
    if (!video_service.loadPage(current_video_query(), requestedPage, page, error_text)) {
        QMessageBox::warning(this, "视频查询失败", error_text);
        return false;
    }

    // 只有查询成功后才提交页码，避免失败时状态与画面不一致。
    current_video_page = page.currentPage;
    video_record_list->clear();
    for (const VideoRecord &record : page.records) {
        QListWidgetItem *item = new QListWidgetItem(
            QStringLiteral("%1 | 通道%2 | %3%4")
                .arg(record.startTime.toString("yyyy-MM-dd HH:mm:ss"))
                .arg(record.channelId)
                .arg(record.name)
                .arg(record.eventVideo ? QStringLiteral(" [事件]") : QString()));
        item->setData(Qt::UserRole, record.path);
        item->setData(Qt::UserRole+1,record.id);
        item->setData(Qt::UserRole+2,record.eventVideo);
        item->setToolTip(QString("通道 %1\n%2\n开始：%3\n结束：%4\n大小：%5 字节\n%6")
            .arg(record.channelId).arg(record.name.toHtmlEscaped())
            .arg(record.startTime.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(record.endTime.toString("yyyy-MM-dd HH:mm:ss")).arg(record.fileSize)
            .arg(record.eventVideo ? "事件录像：选中即可回放" : "普通录像：双击回放"));
        video_record_list->addItem(item);
    }
    lab_video_page->setText(
        QString("%1 / %2 页 · %3 条")
            .arg(page.currentPage)
            .arg(page.totalPages)
            .arg(page.totalRecords)
    );
    btn_previous_video_page->setEnabled(page.currentPage > 1);
    btn_next_video_page->setEnabled(page.currentPage < page.totalPages);
    return true;
}

// 验证权限与选中项，然后按数据库 ID 打开关联录像。
void MainWidget::play_selected_record()
{
    if(!require_permission(AuthPermission::Playback,"播放录像"))return;
    auto item=video_record_list->currentItem();
    if(!item){lab_playback_status->setText("请先选择录像");return;}
    open_linked_video(item->data(Qt::UserRole+1).toLongLong(),0);
}

// 登录后暂停播放器，保留当前源与位置以便继续。
void MainWidget::pause_playback() { if (auth_session.isLoggedIn()) playback_player_->pause(); }
// 仅在登录且有有效录像记录时继续播放，避免退出后恢复旧源。
void MainWidget::resume_playback() { if (auth_session.isLoggedIn() && current_playback_.id > 0) playback_player_->play(); }
// 停止当前播放器；这与停止实时采集或录像线程无关。
void MainWidget::stop_playback() { playback_player_->stop(); }

// 读取组合框中的倍率并设置播放器播放速率，不改变文件编码 FPS。
void MainWidget::change_playback_speed(int index)
{
    Q_UNUSED(index);
    playback_player_->setPlaybackRate(playback_speed_->currentData().toDouble());
}

// 把毫秒位置换算为滑块比例；用户正在拖动时不抢占滑块。
void MainWidget::update_playback_position(qint64 position)
{
    if (!playback_slider_->isSliderDown() && playback_duration_ > 0)
        playback_slider_->setValue(static_cast<int>(position * 1000 / playback_duration_));
}

// 缓存播放器异步报告的时长，供进度显示与拖动定位使用。
void MainWidget::update_playback_duration(qint64 duration)
{
    playback_duration_ = duration;
}

// 把滑块比例换算成毫秒后请求定位；有有效时长才计算。
void MainWidget::seek_playback(int value)
{
    if (playback_duration_ > 0)
        playback_player_->setPosition(playback_duration_ * value / 1000);
}

// 从视频输出当前帧取图，保存真实录像 ID 与当前播放位置。
void MainWidget::take_playback_snapshot()
{
    if(!require_permission(AuthPermission::Playback,"保存回放截图"))return;
    const QImage frame=playback_video_->videoSink()->videoFrame().toImage();
    FeatureImage saved;QString error;
    ImageService service(DbConn::getInstance()->database(),image_directory_);
    if(current_playback_.id<=0 || !service.saveManualImage(frame,current_playback_.channelId,
        QDateTime::currentDateTime(),current_playback_.id,playback_player_->position(),saved,error)){
        lab_playback_status->setText(error.isEmpty() ? "当前没有可保存的回放帧" : error);return;
    }
    audit("截图",saved.imagePath);lab_playback_status->setText("截图已保存到图片管理");
}

// 统一检查会话权限；失败时显示说明，由调用者立即返回。
bool MainWidget::require_permission(AuthPermission permission,
                                    const QString &actionName)
{
    if (auth_session.canAccess(permission)) {
        return true;
    }

    QMessageBox::information(
        this,
        "需要登录",
        QString("请先登录，再%1。登录后才能查看监控画面或进行操作。")
            .arg(actionName)
    );
    return false;
}

// 清除用户和回放状态、隐藏可见画面并回到登录流程；后台采集与录像继续。
void MainWidget::logout()
{
    audit("退出","管理员退出");
    preview_delivery_enabled_.store(false);
    auth_session.logout();
    update_management_access();
    playback_player_->stop();
    playback_player_->setSource(QUrl());
    current_playback_ = VideoRecord();
    pending_playback_offset_ = -1;
    current_video_page = 1;
    video_record_list->clear();
    lab_video_page->setText("第 1 / 1 页，共 0 条");
    lab_login->setText("未登录");
    btn_login->setText("登录");
    btn_monitor_page->setChecked(true);
    page_stack->setCurrentIndex(0);
    for (int i = 0; i < 4; ++i) {
        latest_source_frames_[i].release();
        video_windows[i]->clear();
        video_windows[i]->setText("请先登录");
    }
    // 只锁定客户端界面；后台录像与检测事件不能被注销截断。
    emit login_requested();
    refresh_channel_list();
}

// 请求上一页；有效页码的边界由 load_video_page/VideoService 处理。
void MainWidget::load_previous_video_page()
{
    load_video_page(current_video_page - 1);
}

// 请求下一页，成功后才更新界面显示的当前页。
void MainWidget::load_next_video_page()
{
    load_video_page(current_video_page + 1);
}

// 刷新可读的当前日期时间，不用它衡量采集耗时。
void MainWidget::update_time()
{
    lab_time->setText(
        QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")
    );
}

// 只显示选中通道并更新检测开关；切换布局不销毁其他通道任务。
void MainWidget::show_single_window()
{
    single_mode_=true;
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
void MainWidget::show_multi_window()
{
    single_mode_=false;
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

// 创建每路采集/录像线程及对象，绑定生命周期和帧信箱；真正业务在各接收线程运行。
void MainWidget::init_workers()
{
    if (monitor_threads_[0] || shutting_down_workers_) return;
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

        connect(monitor_threads_[i], &QThread::finished,
                monitor_tasks_[i], &QObject::deleteLater);
        connect(record_threads_[i], &QThread::finished,
                record_tasks_[i], &QObject::deleteLater);
        // 每个消费者最多排队一条通知；慢消费者取最新帧，避免积压全尺寸图像。
        // [this, i] 按值捕获通道下标并保存 this 指针；不复制窗口，也不延长窗口寿命。
        // 外层回调在采集线程立即执行，invokeMethod 的排队回调才在接收对象所属线程执行。
        // DirectConnection 仅访问带锁邮箱，GUI/录像仍在各自线程消费。
        connect(monitor_tasks_[i], &MonitorTask::frameReady, this,
                [this, i](const cv::Mat &frame, int) {
            // 未登录时不向 GUI 排队图像，录像消费者仍正常接收帧。
            if (!preview_delivery_enabled_.load()) return;
            if (preview_mailboxes_[i].publish(frame)) {
                QMetaObject::invokeMethod(this, [this, i] {
                    const cv::Mat latest = preview_mailboxes_[i].take();
                    if (!latest.empty()) display_frame(latest, i + 1);
                }, Qt::QueuedConnection);
            }
        }, Qt::DirectConnection);
        auto recorder = record_tasks_[i];
        connect(monitor_tasks_[i], &MonitorTask::frameReady, recorder,
                [this, i, recorder](const cv::Mat &frame, int) {
            if (!recording_enabled_) return;
            if (recording_mailboxes_[i].publish(frame)) {
                QMetaObject::invokeMethod(recorder, [this, i, recorder] {
                    const cv::Mat latest = recording_mailboxes_[i].take();
                    if (!latest.empty()) recorder->saveFrame(latest, i + 1);
                }, Qt::QueuedConnection);
            }
        }, Qt::DirectConnection);
        connect(monitor_tasks_[i], &MonitorTask::sourceReady,
                this, &MainWidget::on_source_ready, Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::captureStateChanged,
                this, &MainWidget::on_capture_state_changed,
                Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::errorOccurred,
                this, &MainWidget::show_capture_error,
                Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::personDetected,
                this, &MainWidget::on_person_detected, Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::detectionCycleCompleted,
                this, [this](int channel, qint64 elapsed) {
            // 关闭开关时，已进入 GUI 队列的旧统计也不能再对外发布。
            if (!shutting_down_workers_ && detection_allowed_ && detection_enabled_value_)
                emit detectionCycleCompleted(channel, elapsed);
        }, Qt::QueuedConnection);
        connect(monitor_tasks_[i], &MonitorTask::detectionMessage,
                this, &MainWidget::on_detection_message, Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::segmentStarted,
                this, &MainWidget::on_segment_started, Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::segmentSaved,
                this, &MainWidget::on_segment_saved, Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::eventSegmentSaved,this,
            [this](const Video &video,const QString &key,double confidence,const QList<QImage> &frames,const QList<qint64> &offsets,bool complete){
                QString error;MediaRecoveryService events(DbConn::getInstance()->database(),recovery_directory_,image_directory_);
                if(!events.submit(video,key,confidence,frames,offsets,complete,error))
                    lab_storage_warning->setText("事件录像已保存，媒体入库失败："+error);
                else audit("智能事件",key+(complete?" 已完成":" 提前结束"),0);
                const int index=video.getChannelId()-1;
                if(index>=0&&index<4&&active_record_paths_[index]==video.getVideoPath())active_record_paths_[index].clear();
            },Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::recordError,
                this, &MainWidget::on_record_error, Qt::QueuedConnection);
        connect(record_tasks_[i], &RecordTask::storageWarning,
                this, &MainWidget::on_storage_warning, Qt::QueuedConnection);

        monitor_threads_[i]->start();
        record_threads_[i]->start();
    }
}

// 先阻止重复收尾，再停采集、封口录像、处理保存通知，最后释放线程和帧信箱。
void MainWidget::shutdown_workers()
{
    if (shutting_down_workers_) return;
    shutting_down_workers_ = true;
    // 先同步停止采集并释放设备，再退出线程，避免对象销毁时仍在读取帧。
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
        delete monitor_threads_[i]; monitor_threads_[i] = nullptr;
        delete record_threads_[i]; record_threads_[i] = nullptr;
        preview_mailboxes_[i].clear();
        recording_mailboxes_[i].clear();
        latest_source_frames_[i].release();
        preview_running_[i] = false;
        active_record_paths_[i].clear();
        pending_event_keys_[i].clear();
    }
    shutting_down_workers_ = false;
}

// 加载四路配置；失败保留可用默认值并显示原因，不能把损坏数据直接应用。
void MainWidget::load_channel_configs()
{
    QString errorText;
    if (!channel_config_service_.load(channel_configs_, errorText)) {
        // 配置损坏或数据库暂时不可用时，仍允许界面使用安全默认值启动。
        channel_configs_ = ChannelConfigPolicy::defaults();
        video_windows[0]->setText(
            QStringLiteral("配置加载失败，已使用默认值：%1").arg(errorText));
    }
}

// 读取已保存的录像/检测设置并填入控件；兼容配置还有 SystemConfigService 入口。
void MainWidget::load_system_settings()
{
    QString value;
    bool found = false;
    if (config_model_.findValue("record.directory", value, found) && found
            && !value.trimmed().isEmpty()) record_directory_ = value.trimmed();
    if (config_model_.findValue("record.segment_seconds", value, found) && found) {
        const int seconds = value.toInt();
        if (seconds == 10 || seconds == 60) record_segment_seconds_value_ = seconds;
    }
    if (config_model_.findValue("detection.enabled", value, found) && found)
        detection_enabled_value_ = value == QStringLiteral("1");
    if (config_model_.findValue("detection.model_path", value, found) && found)
        yolo_model_path_ = value;
    if (config_model_.findValue("detection.confidence", value, found) && found) {
        bool ok = false;
        const double confidence = value.toDouble(&ok);
        if (ok) detection_confidence_value_ = qBound(0.05, confidence, 0.99);
    }
    record_directory_edit_->setText(record_directory_);
    int segmentRow = record_segment_seconds_->findData(record_segment_seconds_value_);
    if (segmentRow >= 0) record_segment_seconds_->setCurrentIndex(segmentRow);
    detection_enabled_box_->setChecked(detection_allowed_ && detection_enabled_value_);
    yolo_model_edit_->setText(yolo_model_path_);
    detection_confidence_spin_->setValue(detection_confidence_value_);
}

// 选择候选录像目录；取消对话框时保持原值，真正保存由保存按钮触发。
void MainWidget::browse_record_directory()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, "选择录像保存目录", record_directory_edit_->text());
    if (!path.isEmpty()) record_directory_edit_->setText(path);
}

// 先验证并事务保存存储/采集参数，再更新内存并重启正在运行的通道。
void MainWidget::save_recording_settings()
{
    if(!require_permission(AuthPermission::Settings,"修改配置"))return;
    QString path = QDir::cleanPath(record_directory_edit_->text().trimmed());
    QDir dir(path);
    if (path.isEmpty() || (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
            || !QFileInfo(path).isWritable()) {
        QMessageBox::warning(this, "录像设置", "录像目录无法创建或不可写。");
        return;
    }
    const int seconds = record_segment_seconds_->currentData().toInt();
    SystemConfigService service(DbConn::getInstance()->database());
    SystemConfig config;QString error;
    if(!service.load(config,error)){QMessageBox::warning(this,"配置读取失败",error);return;}
    config.videoDirectory=path;config.imageDirectory=image_directory_edit_->text().trimmed();
    config.segmentSeconds=seconds; // 保留旧配置字段，仅兼容数据库，不再控制采集调度。
    if(!service.save(config,error)){QMessageBox::warning(this,"配置保存失败",error);return;}
    image_directory_=QDir(config.imageDirectory).absolutePath();
    capture_interval_value_ = config.captureIntervalMs;
    image_page_->setImageDirectory(image_directory_);
    audit("配置","存储和采集参数已保存");
    record_directory_ = path;
    record_segment_seconds_value_ = seconds;
    QStorageInfo storage(path);
    storage.refresh();
    if (storage.isValid() && storage.bytesTotal() > 0
            && static_cast<double>(storage.bytesAvailable()) / storage.bytesTotal() <= 0.10) {
        on_storage_warning(0, "录像目录所在磁盘剩余空间不超过 10%，请及时清理。");
    }
    for (int i = 0; i < 4; ++i) {
        if (preview_running_[i]) {
            stop_channel(i);
            start_channel(i);
        }
    }
    QMessageBox::information(this, "录像设置", "录像目录和分段时长已保存并应用。");
}

// 选择 ONNX 路径填入控件；此时不加载模型、不启用推理。
void MainWidget::browse_yolo_model()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "选择 YOLO ONNX 模型", yolo_model_edit_->text(), "ONNX 模型 (*.onnx)");
    if (!path.isEmpty()) yolo_model_edit_->setText(path);
}

// 验证模型文件并成组保存检测参数，成功后才重新配置各通道检测。
void MainWidget::save_detection_settings()
{
    if(!require_permission(AuthPermission::Settings,"修改配置"))return;
    const bool enabled = detection_enabled_box_->isChecked();
    const QString path = yolo_model_edit_->text().trimmed();
    const double confidence = detection_confidence_spin_->value();
    if (enabled && !QFileInfo::exists(path)) {
        QMessageBox::warning(this, "智能检测", "启用检测前请选择存在的 ONNX 模型文件。");
        return;
    }
    QString error;
    SystemConfigModel model(DbConn::getInstance()->database());
    if (!model.saveAll({{"detection.enabled", enabled ? "1" : "0"},
                       {"detection.model_path", path},
                       {"detection.confidence", QString::number(confidence)}}, error)) {
        QMessageBox::warning(this, "智能检测", error);
        return;
    }
    detection_enabled_value_ = enabled;
    yolo_model_path_ = path;
    detection_confidence_value_ = confidence;
    apply_detection_mode();
    audit("修改配置", "智能检测设置");
    QMessageBox::information(this, "智能检测", enabled ? "已为配置的通道启用后台人形检测，退出登录后继续运行。" : "智能检测已关闭，预览与普通录像保持运行。");
}

// 接收有效源 FPS，把通道、目录与时长排队传给对应录像 Worker。
void MainWidget::on_source_ready(int channelId, double fps)
{
    if (shutting_down_workers_ || channelId < 1 || channelId > 4) return;
    if (!recording_enabled_ || !record_tasks_[channelId - 1]) return;
    RecordTask *task = record_tasks_[channelId - 1];
    const QString directory = record_directory_;
    const int seconds = record_segment_seconds_value_;
    QMetaObject::invokeMethod(task, [task, channelId, fps, directory, seconds]() {
        task->configure(channelId, fps, directory, seconds);
    }, Qt::QueuedConnection);
}

// 普通段封口后先写恢复清单再入库；失败保留文件和待办并显示警告。
void MainWidget::on_segment_saved(const Video &video)
{
    QString error;
    MediaRecoveryService recovery(DbConn::getInstance()->database(),recovery_directory_,image_directory_);
    if (!recovery.submit(video,QString(),0,{}, {},true,error)) {
        qWarning().noquote() << "录像入库失败：" << error << video.getVideoPath();
        lab_storage_warning->setText(QStringLiteral("录像已保存但数据库入库失败：%1").arg(error));
    }
    if (video.getChannelId() >= 1 && video.getChannelId() <= 4
            && active_record_paths_[video.getChannelId() - 1] == video.getVideoPath())
        active_record_paths_[video.getChannelId() - 1].clear();
}

// 显示录像失败信息，不把失败文件误报为已完成记录。
void MainWidget::on_record_error(int channelId, const QString &message)
{
    qWarning().noquote() << QStringLiteral("通道 %1 录像告警：%2").arg(channelId).arg(message);
    lab_storage_warning->setText(message);
}

// 将空间不足作为系统事件记录并显示，管理员 ID 不归属当前偶然登录用户。
void MainWidget::on_storage_warning(int channelId, const QString &message)
{
    audit("磁盘告警",QString("通道%1：%2").arg(channelId).arg(message),0);
    qWarning().noquote() << message;
    lab_storage_warning->setText(message);
}

// 记录当前正在写入的文件路径，供通道状态与封口回调核对。
void MainWidget::on_segment_started(int channelId, const QString &path)
{
    if(channelId>=1&&channelId<=4)active_record_paths_[channelId-1]=path;
}
// 仅允许当前单画面通道触发，将事件键和置信度投递给录像任务。
void MainWidget::on_person_detected(int channelId,const QString &eventKey,const QDateTime &eventTime,double confidence)
{
    Q_UNUSED(eventTime);
    if(shutting_down_workers_ || !recording_enabled_ || !detection_allowed_ || !detection_enabled_value_ || channelId<1 || channelId>4)return;
    auto task=record_tasks_[channelId-1];
    if(!task)return;
    QMetaObject::invokeMethod(task,[task,eventKey,confidence]{task->beginEvent(eventKey,confidence);},Qt::QueuedConnection);
}

// 展示模型加载/运行状态；检测不可用不等同于采集必然失败。
void MainWidget::on_detection_message(int channelId, const QString &message)
{
    if (!detection_allowed_ || !detection_enabled_value_) return;
    qInfo().noquote() << QStringLiteral("通道 %1：%2").arg(channelId).arg(message);
    lab_storage_warning->setText(message);
}

// 把已应用的配置回填控件，避免把编辑中的值当成正在运行的配置。
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

// 收集四路候选配置，返回值对象供校验；此函数不写数据库。
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

// 按配置和运行标志刷新设备列表，保持编号与数组下标对应。
void MainWidget::refresh_channel_list()
{
    // 刷新运行状态不属于用户换通道，避免重载当前 YOLO 并重置事件状态。
    const QSignalBlocker selectionBlocker(device_list);
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

// 根据摄像头/文件类型启用相关输入，避免两类来源同时编辑。
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

// 为指定通道选择本地文件，取消时保持原路径。
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

// 枚举 Qt 可见摄像头并刷新选择项，实际能否打开由采集任务验证。
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

// 校验并保存四路候选配置，成功后应用到通道；失败不冒充保存成功。
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

    // 保存成功后统一重启预览，让正在运行的通道立即采用新配置。
    for (int i = 0; i < 4; ++i) {
        stop_channel(i);
    }
    channel_configs_ = configs;
    refresh_channel_list();
    start_configured_previews();
    QMessageBox::information(this, "保存成功", "四通道名称和来源已保存并应用。");
}

// 把列表选中项转为业务通道号，更新当前控制目标。
void MainWidget::select_channel(QListWidgetItem *item)
{
    const int row = device_list->row(item);
    if (row < 0 || row >= 4) {
        return;
    }
    selected_channel_ = row + 1;
    lab_video_hint = video_windows[row];
    // 选择只改变显示布局，不重载模型或重置后台检测状态。
    if (single_mode_) show_single_window();
}

// 将用户的启动请求转给当前选中通道。
void MainWidget::start_selected_preview()
{
    if (!auth_session.isLoggedIn()) return;
    start_channel(selected_channel_ - 1);
}

// 将停止请求转给当前通道，同时封口对应录像任务。
void MainWidget::stop_selected_preview()
{
    if (!auth_session.isLoggedIn()) return;
    stop_channel(selected_channel_ - 1);
}

// 启动具有有效配置的通道；单路配置不完整不应阻止其他路。
void MainWidget::start_configured_previews()
{
    if (shutting_down_workers_) return;
    // 各通道分别校验和启动；一个通道未配置不会影响其他通道。
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

// 以值捕获配置快照，排队在采集线程中设置节拍和打开输入；不直接在 GUI 读视频。
void MainWidget::start_channel(int index)
{
    if (shutting_down_workers_) return;
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
    if (!task) return;
    // 排队到对应工作线程执行，主线程只负责发起请求而不会阻塞。
    QMetaObject::invokeMethod(
        task,
        [task, config, interval=capture_interval_value_]() {
            task->configureCaptureInterval(interval);
            task->startCapture(config);
        },
        Qt::QueuedConnection);
    const bool detectionEnabled = detection_allowed_ && detection_enabled_value_;
    const QString modelPath = yolo_model_path_;
    const double confidence = detection_confidence_value_;
    QMetaObject::invokeMethod(task,
        [task, detectionEnabled, modelPath, confidence]() {
            task->configureDetection(detectionEnabled, modelPath, confidence);
        }, Qt::QueuedConnection);
}

// 分别向采集和录像线程发停止请求；排队投递不表示此刻已经停止完成。
void MainWidget::stop_channel(int index)
{
    if (index < 0 || index >= 4 || monitor_tasks_[index] == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(monitor_tasks_[index], "stopCapture",
                              Qt::QueuedConnection);
    if (record_tasks_[index])
        QMetaObject::invokeMethod(record_tasks_[index], "stop",
                                  Qt::QueuedConnection);
}

// 只在 GUI 线程显示信箱交来的最新帧；保留只读源帧供截图，隐藏时跳过绘制。
void MainWidget::display_frame(const cv::Mat &frame, int channelId)
{
    if (!auth_session.isLoggedIn() || shutting_down_workers_ || channelId < 1
            || channelId > 4 || frame.empty() || !preview_running_[channelId - 1]) return;
    const int index = channelId - 1;
    // 保留源帧供全分辨率截图使用，平时不做全帧深复制。
    latest_source_frames_[index] = frame;
    QLabel *target = video_windows[index];
    if (!target->isVisible() || isMinimized()) return;
    const QImage image = frameImage(frame);
    if (image.isNull()) return;
    // 缩小之后再创建 QPixmap，减少全尺寸转换与拷贝。
    target->setPixmap(QPixmap::fromImage(image.scaled(
        target->size(), Qt::KeepAspectRatio, Qt::FastTransformation)));
    emit previewFramePresented(channelId);
}

// 显示对应通道的具体采集错误，并更新列表状态。
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

// 同步采集运行标记；停止时请求录像封口，清空用于截图的旧帧。
void MainWidget::on_capture_state_changed(int channelId, bool running)
{
    if (channelId < 1 || channelId > 4) {
        return;
    }
    running = running && !shutting_down_workers_;
    preview_running_[channelId - 1] = running;
    if (!running) {
        if (record_tasks_[channelId - 1])
            QMetaObject::invokeMethod(record_tasks_[channelId - 1], "stop", Qt::QueuedConnection);
        latest_source_frames_[channelId - 1].release();
        video_windows[channelId - 1]->setText(
            QStringLiteral("%1\n%2")
                .arg(channel_configs_.at(channelId - 1).name,
                     auth_session.isLoggedIn() ? "预览已停止" : "请先登录"));
    }
    refresh_channel_list();
}

// 创建图片/日志页并连接媒体跳转、恢复和备份定时器；这些回调在 GUI 线程。
void MainWidget::init_management_pages()
{
    recovery_directory_ = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("pending-media");
    SystemConfig config;QString error;
    SystemConfigService service(DbConn::getInstance()->database());
    if(!service.load(config,error)) lab_storage_warning->setText(error);
    image_directory_=QDir(config.imageDirectory).absolutePath();
    capture_interval_value_ = config.captureIntervalMs;
    record_directory_ = config.videoDirectory;
    record_segment_seconds_value_ = config.segmentSeconds;
    record_directory_edit_->setText(record_directory_);
    int row = record_segment_seconds_->findData(config.segmentSeconds);
    if(row<0) {
        record_segment_seconds_->addItem(QString("%1 秒").arg(config.segmentSeconds),config.segmentSeconds);
        row=record_segment_seconds_->count()-1;
    }
    record_segment_seconds_->setCurrentIndex(row);
    image_directory_edit_->setText(image_directory_);
    image_page_=new ImagePage(DbConn::getInstance()->database(),image_directory_);
    log_page_=new OperationLogPage(DbConn::getInstance()->database());
    page_stack->addWidget(image_page_);page_stack->addWidget(log_page_);
    connect(images_button_,&QPushButton::clicked,this,[this]{
        if(!require_permission(AuthPermission::Settings,"管理图片"))return;
        image_page_->reload();page_stack->setCurrentWidget(image_page_);
    });
    connect(logs_button_,&QPushButton::clicked,this,[this]{
        if(!require_permission(AuthPermission::Settings,"查看日志"))return;
        log_page_->reload();page_stack->setCurrentWidget(log_page_);
    });
    connect(image_page_,&ImagePage::playbackRequested,this,&MainWidget::open_linked_video);
    connect(log_page_,&OperationLogPage::playbackRequested,this,&MainWidget::open_linked_video);
    connect(log_page_,&OperationLogPage::eventImagesRequested,this,[this](qint64 id){
        if(!require_permission(AuthPermission::Settings,"查看事件图片"))return;
        image_page_->showEventImages(id);page_stack->setCurrentWidget(image_page_);
    });
    connect(image_page_,&ImagePage::imagesExported,this,[this](int count){audit("导出图片",QString::number(count));});
    connect(image_page_,&ImagePage::imagesDeleted,this,[this](int count){audit("删除图片",QString::number(count));});
    auto snapshot=new QPushButton("预览截图",this);snapshot->setObjectName("previewSnapshotButton");
    auto panel = findChild<QFrame*>("devicePanel");
    if (panel && panel->layout()) panel->layout()->addWidget(snapshot);
    connect(snapshot,&QPushButton::clicked,this,&MainWidget::take_preview_snapshot);
    connect(playback_player_,&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus status){
        if(status==QMediaPlayer::LoadedMedia || status==QMediaPlayer::BufferedMedia){
            if(pending_playback_offset_>=0){
                playback_player_->setPosition(qBound<qint64>(0,pending_playback_offset_,playback_player_->duration()));
                pending_playback_offset_=-1;
            }
        }
    });
    auto backup = new BackupService(this);
    auto backupButton = new QPushButton("立即备份数据库", this);
    backupButton->setObjectName("backupDatabaseButton");
    page_stack->widget(2)->layout()->addWidget(backupButton);
    const auto startBackup = [this,backup](int actorId){
        if(backup->running()) return;
        backup->setProperty("actorId",actorId);
        const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString executable = qEnvironmentVariable("SMART_MONITOR_MYSQLDUMP",
             "C:/Program Files/MySQL/MySQL Server 5.7/bin/mysqldump.exe");
        backup->start(DbConn::getInstance()->database(),QDir(root).filePath("backups"),executable);
    };
    connect(backupButton,&QPushButton::clicked,this,[this,startBackup]{
        if(require_permission(AuthPermission::Settings,"备份数据库"))startBackup(auth_session.userId());
    });
    connect(backup,&BackupService::finished,this,[this,backupButton,backup](bool success,const QString &detail){
        backupButton->setEnabled(true);
        lab_storage_warning->setText(detail);
        if(success) {
            QSettings settings;
            settings.setValue("backup/lastSuccess",QDateTime::currentDateTime());
            audit("数据库备份",detail,backup->property("actorId").toInt());
        }
    });
    auto dailyBackup = new QTimer(this);
    dailyBackup->setObjectName("dailyBackupTimer");
    const auto backupIfDue = [backup,startBackup]{
        if (qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") == "0") return;
        QSettings settings;
        const auto last=settings.value("backup/lastSuccess").toDateTime();
        if(!backup->running() && (!last.isValid() || last.secsTo(QDateTime::currentDateTime())>=86400))
            startBackup(0);
    };
    connect(dailyBackup,&QTimer::timeout,this,backupIfDue);
    if(qEnvironmentVariable("SMART_MONITOR_AUTOSTART")!="0") {
        dailyBackup->start(3600000);
        QTimer::singleShot(5000, this, backupIfDue);

    }
    auto recoverTimer=new QTimer(this);
    recoverTimer->setObjectName("mediaRecoveryTimer");
    connect(recoverTimer,&QTimer::timeout,this,[this]{
        if (qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") == "0") return;
        MediaRecoveryService recovery(DbConn::getInstance()->database(),recovery_directory_,image_directory_);
        QStringList errors;const int count=recovery.replay(errors);
        if(!errors.isEmpty())lab_storage_warning->setText("媒体恢复待处理："+errors.first());
        else if(count>0)lab_storage_warning->setText(QString("已恢复 %1 条媒体记录").arg(count));
    });
    if (qEnvironmentVariable("SMART_MONITOR_AUTOSTART", "1") != "0") {
        recoverTimer->start(60000);
        QTimer::singleShot(0, recoverTimer, [recoverTimer]{ QMetaObject::invokeMethod(recoverTimer, "timeout"); });
    }

    update_management_access();
}
// 按登录状态启用或禁用管理入口，避免退出后继续操作旧页面。
void MainWidget::update_management_access()
{
    const bool allowed=auth_session.isLoggedIn();
    for (auto button : {images_button_, logs_button_,
                        btn_playback_page, btn_setting_page, btn_one, btn_four})
        if (button) button->setEnabled(allowed);
    detection_enabled_box_->setEnabled(allowed && detection_allowed_);
    if (auto snapshot = findChild<QPushButton*>("previewSnapshotButton")) snapshot->setEnabled(allowed);
    page_stack->widget(1)->setEnabled(allowed);
    page_stack->widget(2)->setEnabled(allowed);
    if(image_page_)image_page_->setEnabled(allowed);
    if(log_page_)log_page_->setEnabled(allowed);
}
// 写操作日志；actorId 为 0 表示系统，负值使用当前会话，正值使用明确发起者。
void MainWidget::audit(const QString &function,const QString &description, int actorId)
{
    OperationLogService service(DbConn::getInstance()->database());QString error;
    if(!service.record(actorId < 0 ? auth_session.userId() : actorId,function,description,QDateTime::currentDateTime(),error))
        lab_storage_warning->setText("操作成功，审计日志写入失败："+error);
}
// 按录像 ID 查路径并确认文件存在；播放器就绪后定位，同源跳转单独处理。
void MainWidget::open_linked_video(qint64 id,qint64 offset)
{
    if(!require_permission(AuthPermission::Playback,"打开关联录像"))return;
    VideoRecord record;QString error;
    if(!video_service.findById(id,record,error) || !QFileInfo::exists(record.path)){
        playback_player_->stop();
        playback_player_->setSource(QUrl());
        current_playback_ = VideoRecord();
        pending_playback_offset_ = -1;
        // 回放失败必须出现在回放页，不能只更新隐藏的监控页提示。
        page_stack->setCurrentIndex(1);
        btn_playback_page->setChecked(true);
        lab_playback_status->setText(error.isEmpty() ? "录像文件不存在" : error);
        return;
    }
    current_playback_=record;pending_playback_offset_=qMax<qint64>(0,offset);
    lab_playback_status->setText(QStringLiteral("当前录像：%1").arg(record.name));
    page_stack->setCurrentIndex(1);btn_playback_page->setChecked(true);
    playback_player_->stop();
    const QUrl source = QUrl::fromLocalFile(record.path);
    if (playback_player_->source() == source) {
        playback_player_->setPosition(pending_playback_offset_);
        pending_playback_offset_ = -1;
    } else {
        playback_player_->setSource(source);
    }
    playback_player_->setPlaybackRate(playback_speed_->currentData().toDouble());
    playback_player_->play();audit("回放",record.path);
}
// 从选中通道保存最近源帧；当前录像尚未入库时不编造 videoId。
void MainWidget::take_preview_snapshot()
{
    if(!require_permission(AuthPermission::Settings,"保存截图"))return;
    const int index=selected_channel_-1;
    FeatureImage saved;QString error;
    ImageService service(DbConn::getInstance()->database(),image_directory_);
    if(!service.saveManualImage(frameImage(latest_source_frames_[index]).copy(),selected_channel_,QDateTime::currentDateTime(),0,0,saved,error)){
        lab_storage_warning->setText(error);return;
    }
    audit("截图",saved.imagePath);lab_storage_warning->setText("截图已保存到图片管理");
}

// 把全局开关和模型参数同步到所有通道，布局与选中项不改变检测范围。
void MainWidget::apply_detection_mode()
{
    for(int i=0;i<4;++i){
        auto task=monitor_tasks_[i];if(!task)continue;
        const bool enabled=detection_allowed_ && detection_enabled_value_;
        const QString path=yolo_model_path_;const double confidence=detection_confidence_value_;
        QMetaObject::invokeMethod(task,[task,enabled,path,confidence]{task->configureDetection(enabled,path,confidence);},Qt::QueuedConnection);
    }
}
