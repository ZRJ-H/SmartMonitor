// 【阅读导引】
// 窗口协调：持有主窗口和登录窗口，用信号转交认证结果。
// 当前未登录时显示登录窗口，取消登录退出应用；隐藏主窗口不停止后台采集。

// 实现主窗口与登录窗口的生命周期管理，以及登录信号到界面切换流程的衔接。
#include "windowmanager.h"
#include "../view/loginwidget.h"
#include "../view/mainwidget.h"

#include <QApplication>

WindowManager *WindowManager::instance = nullptr;

WindowManager *WindowManager::getInstance()
{
    if (instance == nullptr) {
        Q_ASSERT(QApplication::instance() != nullptr);
        instance = new WindowManager;
    }
    return instance;
}

// 释放管理器持有的顶层窗口，再把单例指针置空。
void WindowManager::releaseInstance()
{
    delete instance;
    instance = nullptr;
}

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
WindowManager::WindowManager(QObject *parent)
    : QObject(parent),
      main_widget(new MainWidget),
      login_widget(new LoginWidget)
{
    // 窗口只通过信号交流，避免 MainWidget 与 LoginWidget 互相持有指针。
    connect(main_widget, &MainWidget::login_requested,
            this, &WindowManager::showLoginWindow);
    connect(login_widget, &LoginWidget::login_success,
            this, &WindowManager::onLoginSuccess);
    connect(login_widget, &LoginWidget::login_cancelled,
            this, &WindowManager::onLoginCancelled);
}

// 对象销毁时释放本类负责的资源；依赖的外部对象需按约定保持有效。
WindowManager::~WindowManager()
{
    delete login_widget;
    login_widget = nullptr;
    delete main_widget;
    main_widget = nullptr;
}

// 必须已登录才显示主窗口，否则转到登录窗口。
void WindowManager::showMainWindow()
{
    if (!main_widget->isLoggedIn()) {
        showLoginWindow();
        return;
    }
    main_widget->show();
    main_widget->raise();
    main_widget->activateWindow();
}

// 隐藏主窗口并显示登录窗口；隐藏不会销毁后台 Worker。
void WindowManager::showLoginWindow()
{
    main_widget->hide();
    login_widget->show();
    login_widget->raise();
    login_widget->activateWindow();
}

// 转交真实用户 ID 和名称，更新主窗口状态后展示。
void WindowManager::onLoginSuccess(int userId, const QString &userName)
{
    login_widget->hide();
    main_widget->setLoggedInUser(userId, userName);
    showMainWindow();
}

// 取消登录时退出应用事件循环，随后由 main 统一释放资源。
void WindowManager::onLoginCancelled()
{
    login_widget->hide();
    QApplication::quit();
}
