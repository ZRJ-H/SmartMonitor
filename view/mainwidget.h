#ifndef MAINWIDGET_H
#define MAINWIDGET_H
#include "../service/authsession.h"
#include <QWidget>
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class MonitorPage;
class PlaybackPage;
class SettingsPage;
class ImagePage;
class OperationLogPage;
class MonitoringController;
class MaintenanceController;
// 只组装页面、连接模块和维护会话，不持有播放器、工作线程或页面控件。
class MainWidget : public QWidget {
    Q_OBJECT
public:
    explicit MainWidget(QWidget* parent = nullptr);
    ~MainWidget() override;
    void setLoggedInUser(int id, const QString& name);
    bool isLoggedIn() const
    {
        return auth_session.isLoggedIn();
    }
signals:
    void login_requested();
    void previewFramePresented(int channel);
    void detectionCycleCompleted(int channel, qint64 elapsed);
private slots:
    void show_monitor_page();
    void show_playback_page();
    void show_setting_page();
    void handle_login_button();
    void update_time();

private:
    void init_ui();
    void init_connect();
    void init_showtime();
    void update_management_access();
    void updateImageDirectory(const QString& path);
    void audit(const QString&, const QString&, int actorId = -1);
    bool require_permission(AuthPermission, const QString&);
    void logout();
    MonitorPage* monitor_page_ = nullptr;
    PlaybackPage* playback_page_ = nullptr;
    SettingsPage* settings_page_ = nullptr;
    ImagePage* image_page_ = nullptr;
    OperationLogPage* log_page_ = nullptr;
    MonitoringController* monitoring_ = nullptr;
    MaintenanceController* maintenance_ = nullptr;
    QPushButton *btn_monitor_page = nullptr, *btn_playback_page = nullptr, *btn_setting_page = nullptr,
                *btn_login = nullptr, *images_button_ = nullptr, *logs_button_ = nullptr;
    QLabel *lab_login = nullptr, *lab_time = nullptr;
    QStackedWidget* page_stack = nullptr;
    QTimer* m_timer = nullptr;
    AuthSession auth_session;
};
#endif
