#include "loginwidget.h"
#include "..\util\verifycodelabel.h"

#include <QBrush>
#include <QCloseEvent>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

LoginWidget::LoginWidget(QWidget *parent)
    : QWidget(parent),
      // 输入框和验证码控件最终都会加入窗口布局，由 Qt 父子对象机制释放。
      edit_user(new QLineEdit),
      edit_pwd(new QLineEdit),
      edit_vcode(new QLineEdit),
      pic_vcode(new VerifyCodeLabel),
      user_service()
{
    setWindowTitle("登录窗口");
    resize(800, 600);
    setMinimumSize(680, 500);

    // 资源路径以 :/ 开头，图片来自 resources.qrc，而不是运行目录中的普通文件。
    const QPixmap background(":/picture/OIP-C3.webp");
    if (!background.isNull()) {
        QPalette palette = this->palette();
        palette.setBrush(
            QPalette::Window,
            QBrush(background.scaled(
                size(),
                // 拉伸图片铺满窗口；SmoothTransformation 提高缩放质量。
                Qt::IgnoreAspectRatio,
                Qt::SmoothTransformation
            ))
        );
        setAutoFillBackground(true);
        setPalette(palette);
    }

    // 最外层采用垂直布局，利用上下 stretch 让表单保持在窗口中间。
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(70, 55, 70, 55);

    QLabel *lab_title = new QLabel("XX系统登录窗口");
    lab_title->setObjectName("loginTitle");
    lab_title->setFont(QFont("宋体", 20, QFont::Bold));
    lab_title->setAlignment(Qt::AlignCenter);

    // form_panel 是半透明的表单卡片，内部再使用垂直布局组织表单和按钮。
    QFrame *form_panel = new QFrame;
    form_panel->setObjectName("formPanel");
    form_panel->setMaximumWidth(520);
    QVBoxLayout *panel_layout = new QVBoxLayout(form_panel);
    panel_layout->setContentsMargins(46, 34, 46, 34);
    panel_layout->setSpacing(18);

    // QFormLayout 适合“左侧标签 + 右侧输入控件”的表单结构。
    QFormLayout *form_layout = new QFormLayout;
    form_layout->setSpacing(20);
    form_layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    edit_user->setPlaceholderText("请输入用户名");
    edit_pwd->setPlaceholderText("请输入密码");

    // Password 模式只改变界面显示，不等于对密码进行了加密。
    edit_pwd->setEchoMode(QLineEdit::Password);
    form_layout->addRow("用户名：", edit_user);
    form_layout->addRow("密  码：", edit_pwd);

    // 验证码这一行中同时放输入框、验证码图片和一个伸缩项。
    QHBoxLayout *vcode_layout = new QHBoxLayout;
    edit_vcode->setPlaceholderText("验证码");
    edit_vcode->setMaxLength(4);
    edit_vcode->setMaximumWidth(110);
    vcode_layout->addWidget(edit_vcode);
    vcode_layout->addWidget(pic_vcode);
    vcode_layout->addStretch();
    form_layout->addRow("验证码：", vcode_layout);
    panel_layout->addLayout(form_layout);

    QLabel *lab_tip = new QLabel(
        "提示：验证码通过后，将从数据库校验用户名和密码"
    );
    lab_tip->setObjectName("tipLabel");
    lab_tip->setWordWrap(true);
    panel_layout->addWidget(lab_tip);

    QHBoxLayout *button_layout = new QHBoxLayout;
    QPushButton *btn_login = new QPushButton("登录");
    QPushButton *btn_reg = new QPushButton("注册");
    QPushButton *btn_cancel = new QPushButton("取消");
    btn_login->setObjectName("primaryButton");

    button_layout->addWidget(btn_login);
    button_layout->addWidget(btn_reg);
    button_layout->addWidget(btn_cancel);
    panel_layout->addLayout(button_layout);

    main_layout->addStretch();
    main_layout->addWidget(lab_title);
    main_layout->addSpacing(24);
    main_layout->addWidget(form_panel, 0, Qt::AlignHCenter);
    main_layout->addStretch();

    // objectName 让同一种控件能够使用不同的 QSS 样式。
    setStyleSheet(
        "QLabel#loginTitle { color: white; font-size: 30px; }"
        "QFrame#formPanel { background-color: rgba(255, 255, 255, 218); "
        "border: 1px solid rgba(255, 255, 255, 170); border-radius: 14px; }"
        "QFrame#formPanel QLabel { color: #263238; font: 16px 'Microsoft YaHei'; }"
        "QLineEdit { min-height: 38px; border: 1px solid #b7c1cc; "
        "border-radius: 6px; padding: 0 10px; background-color: white; "
        "color: #1d2732; font: 15px 'Microsoft YaHei'; }"
        "QLineEdit:focus { border: 2px solid #3778d0; }"
        "QPushButton { min-height: 38px; border: 1px solid #a9b4c0; "
        "border-radius: 6px; background-color: #f4f6f8; color: #263238; "
        "font: 15px 'Microsoft YaHei'; }"
        "QPushButton:hover { background-color: #e2e8ef; }"
        "QPushButton#primaryButton { color: white; background-color: #2f6fc2; "
        "border-color: #2f6fc2; }"
        "QPushButton#primaryButton:hover { background-color: #3d82da; }"
        "QLabel#tipLabel { color: #56616d; font-size: 13px; }"
    );

    // 一个槽可以接收多个信号：点击登录按钮或在输入框按回车都会执行 login()。
    connect(btn_login, SIGNAL(clicked()),
            this, SLOT(login()));
    connect(edit_pwd, SIGNAL(returnPressed()),
            this, SLOT(login()));
    connect(edit_vcode, SIGNAL(returnPressed()),
            this, SLOT(login()));

    connect(btn_cancel, SIGNAL(clicked()),
            this, SLOT(cancel()));

    // 函数指针 + Lambda 是现代 connect 写法。
    // this 是 Lambda 的上下文对象，窗口销毁后该连接会自动断开。
    connect(btn_reg, &QPushButton::clicked,
            this, [this]() {
        QMessageBox::information(
            this,
            "提示",
            "本章示例主要演示信号和槽，注册功能暂未开放。"
        );
    });
}

void LoginWidget::login()
{
    // trimmed() 去掉用户名和验证码首尾空格，密码则保留用户原始输入。
    const QString user_name = edit_user->text().trimmed();
    const QString password = edit_pwd->text();
    const QString input_code = edit_vcode->text().trimmed();

    // 校验失败时立即 return，后续成功逻辑不会继续执行。
    if (user_name.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户名和密码不能为空。");
        return;
    }

    // CaseInsensitive 表示验证码比较时不区分大小写。
    if (input_code.compare(
            pic_vcode->getVerificationCode(),
            Qt::CaseInsensitive
        ) != 0) {
        QMessageBox::warning(this, "提示", "验证码错误，请重新输入。");

        // 错误后清空输入、刷新验证码并把输入焦点还给验证码输入框。
        edit_vcode->clear();
        pic_vcode->updateVerificationCode();
        edit_vcode->setFocus();
        return;
    }

    // 验证码通过后交给 Service 完成账号校验和登录时间更新。
    const LoginResult result = user_service.login(user_name, password);

    if (result.status == LoginResult::DatabaseError) {
        QMessageBox::critical(this, "数据库操作失败", result.errorText);
        edit_vcode->clear();
        pic_vcode->updateVerificationCode();
        edit_vcode->setFocus();
        return;
    }

    if (result.status == LoginResult::InvalidCredentials) {
        QMessageBox::warning(this, "登录失败", "用户名或密码错误。");
        edit_pwd->clear();
        edit_vcode->clear();
        pic_vcode->updateVerificationCode();
        edit_pwd->setFocus();
        return;
    }

    QMessageBox::information(this, "登录成功", "验证通过，正在返回主界面。");
    emit login_success(result.userName);

    // 清理敏感信息并刷新验证码，为下一次打开登录窗口做准备。
    edit_pwd->clear();
    edit_vcode->clear();
    pic_vcode->updateVerificationCode();
}

void LoginWidget::cancel()
{
    // 只发布取消事件，窗口切换由 WindowManager 处理。
    emit login_cancelled();
}

void LoginWidget::closeEvent(QCloseEvent *event)
{
    // 标题栏关闭按钮与“取消”按钮采用相同的状态通知。
    emit login_cancelled();

    // 接受关闭事件，让本次关闭继续执行。
    event->accept();
}
