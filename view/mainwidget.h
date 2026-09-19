#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include "../service/authsession.h"
#include "../service/channelconfigservice.h"
#include "../service/videoservice.h"

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
class MonitorTask;

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);
    ~MainWidget() override;

    void setLoggedInUser(const QString &userName);

signals:
    void login_requested();

private slots:
    void show_monitor_page();
    void show_playback_page();
    void show_setting_page();
    void handle_login_button();
    void update_time();
    void show_single_window();
    void show_multi_window();
    void load_video_records();
    void load_previous_video_page();
    void load_next_video_page();

    void select_channel(QListWidgetItem *item);
    void start_selected_preview();
    void stop_selected_preview();
    void display_frame(const cv::Mat &frame, int channelId);
    void show_capture_error(int channelId, const QString &message);
    void on_capture_state_changed(int channelId, bool running);
    void refresh_camera_devices();
    void save_channel_configs();

private:
    void init_ui();
    void init_style();
    void init_connect();
    void init_showtime();
    void init_workers();
    void shutdown_workers();
    void load_channel_configs();
    void apply_channel_configs_to_ui();
    QList<ChannelConfig> collect_channel_configs_from_ui() const;
    void refresh_channel_list();
    void update_source_controls(int index);
    void choose_channel_file(int index);
    void start_configured_previews();
    void start_channel(int index);
    void stop_channel(int index);
    bool load_video_page(int requestedPage);
    bool require_permission(AuthPermission permission,
                            const QString &actionName);
    void logout();

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

    QLabel *lab_login;
    QLabel *lab_time;
    QLabel *lab_video_hint;

    QStackedWidget *page_stack;
    QListWidget *device_list;
    QListWidget *video_record_list;
    QLabel *lab_video_page;
    QPushButton *btn_previous_video_page;
    QPushButton *btn_next_video_page;
    VideoService video_service;
    int current_video_page;
    AuthSession auth_session;

    QLabel *video_windows[4];
    QGridLayout *video_layout;
    QTimer *m_timer;

    // 四组配置控件与 channel_configs_ 的下标一一对应。
    QLineEdit *channel_name_edits_[4];
    QComboBox *channel_source_boxes_[4];
    QComboBox *channel_camera_boxes_[4];
    QLineEdit *channel_file_edits_[4];
    QPushButton *channel_file_buttons_[4];
    QMediaDevices *media_devices_;
    QList<ChannelConfig> channel_configs_;
    ChannelConfigService channel_config_service_;

    // 每个通道拥有独立线程和任务，一个数据源失败不会阻塞其他通道。
    QThread *monitor_threads_[4];
    MonitorTask *monitor_tasks_[4];
    bool preview_running_[4];
    int selected_channel_;
};

#endif // MAINWIDGET_H
