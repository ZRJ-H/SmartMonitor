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

void WindowManager::onLoginSuccess(const QString &userName)
{
    login_widget->hide();
    main_widget->setLoggedInUser(userName);
    showMainWindow();
}

void WindowManager::onLoginCancelled()
{
    login_widget->hide();
    showMainWindow();
}
