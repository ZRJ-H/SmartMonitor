// 窗口管理器：统一持有主窗口和登录窗口，负责二者之间的显示切换与登录结果传递。
#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QString>

class LoginWidget;
class MainWidget;

// 应用级窗口协调器。使用单例保证整个程序只有一组主窗口和登录窗口。
class WindowManager : public QObject
{
    Q_OBJECT

public:
    // 返回窗口管理器单例；必须在 QApplication 创建后调用。
    static WindowManager *getInstance();
    // 销毁窗口及管理器单例，供程序退出阶段释放资源。
    static void releaseInstance();

    // 显示并激活主窗口。
    void showMainWindow();

    WindowManager(const WindowManager &) = delete;
    WindowManager &operator=(const WindowManager &) = delete;

private slots:
    // 响应主窗口的登录请求：隐藏主窗口并显示登录窗口。
    void showLoginWindow();
    // 登录成功后记录用户名、关闭登录窗口并恢复主窗口。
    void onLoginSuccess(int userId, const QString &userName);
    // 用户取消登录时关闭登录窗口并恢复主窗口。
    void onLoginCancelled();

private:
    explicit WindowManager(QObject *parent = nullptr);
    ~WindowManager() override;

    static WindowManager *instance;  // 全局唯一的窗口管理器实例。
    MainWidget *main_widget;         // 应用主界面，由本类创建并负责销毁。
    LoginWidget *login_widget;       // 登录界面，由本类创建并负责销毁。
};

#endif // WINDOWMANAGER_H
