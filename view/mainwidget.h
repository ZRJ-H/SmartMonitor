// 声明智能监控主窗口，组织实时预览、录像回放、通道设置、登录权限和四路采集线程。
#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include "../service/authsession.h"
#include "../service/channelconfigservice.h"
#include "../service/videoservice.h"
#include "../model/configmodel.h"

#include <QList>
#include <QString>
#include <QWidget>

#include <opencv2/core.hpp>

class QLabel;
class QPushButton;
class QTimer;
class QGridLayout;
class QStackedWidget;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QLineEdit;
class QMediaDevices;
class QThread;
class QDateTimeEdit;
class QCheckBox;
class QDoubleSpinBox;
class QSlider;
class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class MonitorTask;
class RecordTask;

// 应用主界面：只在 GUI 线程更新控件，视频读取由四个 MonitorTask 工作线程完成。
class MainWidget : public QWidget
{
    Q_OBJECT

public:
    // 构建三类页面、加载配置并启动可用通道的预览任务。
    explicit MainWidget(QWidget *parent = nullptr);
    // 停止所有采集任务并等待线程安全退出。
    ~MainWidget() override;

    // 登录成功后建立主窗口会话并更新登录按钮/标签。
    void setLoggedInUser(const QString &userName);

signals:
    // 未登录用户请求登录时发出，由 WindowManager 切换到登录窗口。
    void login_requested();

private slots:
    // 以下三个槽切换堆叠页面；回放和设置页会先检查登录权限。
    void show_monitor_page();
    void show_playback_page();
    void show_setting_page();
    // 根据当前会话执行“请求登录”或“退出登录”。
    void handle_login_button();
    // 每秒刷新标题栏的系统时间。
    void update_time();
    // 将选中通道放大为单画面，或恢复 2×2 四画面布局。
    void show_single_window();
    void show_multi_window();
    // 加载录像当前页，以及处理上一页、下一页按钮。
    void load_video_records();
    void load_previous_video_page();
    void load_next_video_page();
    void play_selected_record();
    void pause_playback();
    void resume_playback();
    void stop_playback();
    void take_playback_snapshot();
    void seek_playback(int value);
    void update_playback_position(qint64 position);
    void update_playback_duration(qint64 duration);
    void change_playback_speed(int index);

    // 更新当前选中的通道和对应画面标签。
    void select_channel(QListWidgetItem *item);
    // 启动或停止当前选中通道的预览。
    void start_selected_preview();
    void stop_selected_preview();
    // 接收工作线程帧，转换颜色空间后显示到 channelId 对应窗口。
    void display_frame(const cv::Mat &frame, int channelId);
    // 在对应画面显示采集错误，并同步通道运行状态。
    void show_capture_error(int channelId, const QString &message);
    // 接收任务运行状态，刷新通道列表和停止提示。
    void on_capture_state_changed(int channelId, bool running);
    // 重新枚举系统摄像头，并尽量保留原先选择。
    void refresh_camera_devices();
    // 校验并事务保存四路配置，成功后重启预览使新配置生效。
    void save_channel_configs();
    void browse_record_directory();
    void save_recording_settings();
    void browse_yolo_model();
    void save_detection_settings();
    void on_source_ready(int channelId, double fps);
    void on_segment_saved(const Video &video);
    void on_record_error(int channelId, const QString &message);
    void on_storage_warning(int channelId, const QString &message);
    void on_segment_started(int channelId, const QString &path);
    void on_person_detected(int channelId, const QString &eventKey,
                            const QDateTime &eventTime, double confidence);
    void on_detection_message(int channelId, const QString &message);

private:
    // 分别创建控件、应用样式、建立信号槽、启动时钟和工作线程。
    void init_ui();
    void init_style();
    void init_connect();
    void init_showtime();
    void init_workers();
    // 同步停止采集，再退出并等待所有工作线程。
    void shutdown_workers();
    // 从数据库加载配置；失败时退回安全默认值。
    void load_channel_configs();
    // 在配置对象与四组设置控件之间双向同步。
    void apply_channel_configs_to_ui();
    QList<ChannelConfig> collect_channel_configs_from_ui() const;
    // 根据配置与运行状态重建左侧通道列表。
    void refresh_channel_list();
    // 按数据源类型启用摄像头选择或文件选择控件。
    void update_source_controls(int index);
    // 为指定通道打开视频文件选择对话框。
    void choose_channel_file(int index);
    // 尝试启动所有配置完整的通道；单路失败不影响其他通道。
    void start_configured_previews();
    // 通过队列调用在对应线程中启动/停止指定下标的采集任务。
    void start_channel(int index);
    void stop_channel(int index);
    // 加载 requestedPage，成功返回 true，并同步列表、页码和翻页按钮。
    bool load_video_page(int requestedPage);
    // 检查权限；不足时显示登录提示并返回 false，允许访问返回 true。
    bool require_permission(AuthPermission permission,
                            const QString &actionName);
    // 清除会话并将受保护页面切回实时预览。
    void logout();
    void load_system_settings();
    VideoQuery current_video_query() const;

    // 顶部导航、登录及预览控制按钮。
    QPushButton *btn_monitor_page;
    QPushButton *btn_playback_page;
    QPushButton *btn_setting_page;
    QPushButton *btn_login;
    QPushButton *btn_one;
    QPushButton *btn_four;
    QPushButton *btn_refresh_records;
    QPushButton *btn_start_preview;
    QPushButton *btn_stop_preview;
    QPushButton *btn_refresh_cameras;
    QPushButton *btn_save_channel_configs;
    QPushButton *btn_play_record;
    QPushButton *btn_pause_playback;
    QPushButton *btn_resume_playback;
    QPushButton *btn_stop_playback;
    QPushButton *btn_playback_snapshot;
    QPushButton *btn_save_recording_settings;
    QPushButton *btn_save_detection_settings;

    // 顶栏登录/时间文本以及当前选中视频窗口的提示标签。
    QLabel *lab_login;
    QLabel *lab_time;
    QLabel *lab_video_hint;
    QLabel *lab_storage_warning;
    QLabel *lab_playback_status;

    QStackedWidget *page_stack;       // 承载实时预览、录像回放和设置三个页面。
    QListWidget *device_list;         // 四个监控通道及其运行状态列表。
    QListWidget *video_record_list;   // 当前页录像文件名列表。
    QLabel *lab_video_page;           // 当前页/总页数提示。
    QPushButton *btn_previous_video_page; // 录像上一页按钮。
    QPushButton *btn_next_video_page;     // 录像下一页按钮。
    QDateTimeEdit *query_start_time_;
    QDateTimeEdit *query_end_time_;
    QComboBox *query_channel_;
    QVideoWidget *playback_video_;
    QMediaPlayer *playback_player_;
    QAudioOutput *playback_audio_;
    QSlider *playback_slider_;
    QComboBox *playback_speed_;
    qint64 playback_duration_;
    VideoService video_service;       // 提供录像分页数据。
    int current_video_page;           // 当前已成功加载的录像页码。
    AuthSession auth_session;         // 主窗口当前登录身份与权限状态。

    QLabel *video_windows[4]; // 四个视频画面，数组下标与通道下标一致。
    QGridLayout *video_layout; // 单画面和 2×2 画面共用的布局。
    QTimer *m_timer;           // 每秒触发 update_time() 的界面时钟。

    // 四组配置控件与 channel_configs_ 的下标一一对应。
    QLineEdit *channel_name_edits_[4];      // 通道显示名称。
    QComboBox *channel_source_boxes_[4];    // 摄像头/本地文件来源类型。
    QComboBox *channel_camera_boxes_[4];    // 可用摄像头及其索引。
    QLineEdit *channel_file_edits_[4];      // 本地视频文件路径。
    QPushButton *channel_file_buttons_[4];  // 打开文件选择对话框。
    QMediaDevices *media_devices_;          // 监听系统视频输入设备变化。
    QList<ChannelConfig> channel_configs_;  // 当前已应用的四路配置快照。
    ChannelConfigService channel_config_service_; // 配置加载与事务保存服务。
    ConfigModel config_model_;
    QLineEdit *record_directory_edit_;
    QComboBox *record_segment_seconds_;
    QCheckBox *detection_enabled_box_;
    QLineEdit *yolo_model_edit_;
    QDoubleSpinBox *detection_confidence_spin_;
    QString record_directory_;
    int record_segment_seconds_value_;
    bool detection_enabled_value_;
    QString yolo_model_path_;
    double detection_confidence_value_;

    // 每个通道拥有独立线程和任务，一个数据源失败不会阻塞其他通道。
    QThread *monitor_threads_[4];  // 每路采集任务所属的工作线程。
    MonitorTask *monitor_tasks_[4]; // 每路视频源的实际采集对象。
    QThread *record_threads_[4];
    RecordTask *record_tasks_[4];
    QString active_record_paths_[4];
    QString pending_event_keys_[4];
    QDateTime pending_event_times_[4];
    double pending_event_confidences_[4];
    bool preview_running_[4];       // 四个通道当前是否正在采集。
    int selected_channel_;          // 当前选中通道编号，范围为 1～4。
};

#endif // MAINWIDGET_H
