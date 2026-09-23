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

void WindowManager::releaseInstance()
{
    delete instance;
    instance = nullptr;
}

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

WindowManager::~WindowManager()
{
    delete login_widget;
    login_widget = nullptr;
    delete main_widget;
    main_widget = nullptr;
}

void WindowManager::showMainWindow()
{
    main_widget->show();
    main_widget->raise();
    main_widget->activateWindow();
}

void WindowManager::showLoginWindow()
{
    main_widget->hide();
    login_widget->show();
    login_widget->raise();
    login_widget->activateWindow();
}

void WindowManager::onLoginSuccess(int userId, const QString &userName)
{
    login_widget->hide();
    main_widget->setLoggedInUser(userId, userName);
    showMainWindow();
}

void WindowManager::onLoginCancelled()
{
    login_widget->hide();
    showMainWindow();
}
