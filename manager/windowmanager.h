#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QString>

class LoginWidget;
class MainWidget;

class WindowManager : public QObject
{
    Q_OBJECT

public:
    static WindowManager *getInstance();
    static void releaseInstance();

    void showMainWindow();

    WindowManager(const WindowManager &) = delete;
    WindowManager &operator=(const WindowManager &) = delete;

private slots:
    void showLoginWindow();
    void onLoginSuccess(const QString &userName);
    void onLoginCancelled();

private:
    explicit WindowManager(QObject *parent = nullptr);
    ~WindowManager() override;

    static WindowManager *instance;
    MainWidget *main_widget;
    LoginWidget *login_widget;
};

#endif // WINDOWMANAGER_H
