#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include "../service/videoservice.h"

#include <QString>
#include <QWidget>

// 这里只需要指针，因此使用前置声明可以减少头文件之间的依赖。
class QLabel;
class QPushButton;
class QTimer;
class QGridLayout;
class QStackedWidget;
class QListWidget;

// 监控系统主界面：负责页面导航、登录跳转、时间显示和监控画面布局切换。
class MainWidget : public QWidget
{
    // 自定义槽依赖 Qt 元对象系统，因此类中必须有 Q_OBJECT。
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);
    ~MainWidget() override;

    void setLoggedInUser(const QString &userName);

signals:
    void login_requested();

private slots:
    // 顶部三个导航按钮对应的页面切换槽。
    void show_monitor_page();
    void show_playback_page();
    void show_setting_page();

    // 主界面 → 登录窗口。
    void to_login_win();

    // QTimer 每秒触发该槽，用当前时间更新标签。
    void update_time();

    // 两个按钮对应的监控布局切换槽。
    void show_single_window();

    // 登录成功后或点击分页按钮时，通过 VideoService 加载文件名。
    void load_video_records();
    void load_previous_video_page();
    void load_next_video_page();
    void show_multi_window();

private:
    // 将创建界面、设置样式、建立连接和启动定时器拆开，便于阅读维护。
    void init_ui();
    void init_style();
    void init_connect();
    void init_showtime();
    bool load_video_page(int requestedPage);

    // 顶部导航按钮需要在 init_connect() 中建立页面切换连接。
    QPushButton *btn_monitor_page;
    QPushButton *btn_playback_page;
    QPushButton *btn_setting_page;

    // 需要在多个成员函数中访问的功能按钮。
    QPushButton *btn_login;
    QPushButton *btn_one;
    QPushButton *btn_four;
    QPushButton *btn_refresh_records;

    // 顶部登录状态、实时时间和第一监控窗口的提示文字。
    QLabel *lab_login;
    QLabel *lab_time;
    QLabel *lab_video_hint;

    // QStackedWidget 同一时间只显示一个页面，通过索引切换监控、回放和设置页。

    QStackedWidget *page_stack;
    QListWidget *video_record_list;
    QLabel *lab_video_page;
    QPushButton *btn_previous_video_page;
    QPushButton *btn_next_video_page;
    VideoService video_service;
    int current_video_page;

    // 未登录时不允许查询监控录像。
    bool logged_in;


    // 暂用四个 QLabel 模拟四路视频窗口，重点演示 QGridLayout 的切换。
    QLabel *video_windows[4];
    QGridLayout *video_layout;

    // m_timer 以 MainWidget 为父对象，会跟随主窗口自动销毁。
    QTimer *m_timer;
};

#endif // MAINWIDGET_H
