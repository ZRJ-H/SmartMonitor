#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include "../service/userservice.h"

#include <QString>
#include <QWidget>

class QCloseEvent;
class QLineEdit;
class VerifyCodeLabel;

// 登录窗口只负责收集和校验输入，通过信号把结果通知主窗口。
class LoginWidget : public QWidget
{
    // login_success/login_cancelled 是自定义信号，必须启用元对象系统。
    Q_OBJECT

public:
    explicit LoginWidget(QWidget *parent = nullptr);

signals:
    // 自定义信号只声明，不手写定义；moc 会生成信号实现。
    // 信号表示“事情已经发生”，而不直接操作 MainWidget，从而降低耦合。
    void login_success(const QString &userName);
    void login_cancelled();

public slots:
    // 登录按钮和输入框回车都复用 login()，避免重复编写校验代码。
    void login();
    void cancel();

protected:
    // 点窗口右上角关闭按钮时，也要通知主界面恢复显示。
    void closeEvent(QCloseEvent *event) override;

private:
    // 这些控件需要在 login() 中继续访问，因此保存为成员变量。
    QLineEdit *edit_user;
    QLineEdit *edit_pwd;
    QLineEdit *edit_vcode;
    VerifyCodeLabel *pic_vcode;

    // View 只调用 Service，不直接访问数据库。
    UserService user_service;
};

#endif // LOGINWIDGET_H
