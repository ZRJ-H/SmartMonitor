// 实现登录表单布局、输入检查、验证码校验和 LoginResult 到用户提示的映射。
#include "loginwidget.h"
#include "apptheme.h"
#include "..\util\verifycodelabel.h"

#include <QDialog>
#include <QDialogButtonBox>
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

    QLabel *lab_title = new QLabel("安防监控系统登录");
    lab_title->setObjectName("loginTitle");
    lab_title->setFont(QFont("宋体", 20, QFont::Bold));
    lab_title->setAlignment(Qt::AlignCenter);

    // 实色卡片同时承载标题和表单，背景照片不会干扰文字阅读。
    QFrame *form_panel = new QFrame;
    form_panel->setObjectName("formPanel");
    form_panel->setMaximumWidth(560);
    QVBoxLayout *panel_layout = new QVBoxLayout(form_panel);
    panel_layout->setContentsMargins(46, 34, 46, 34);
    panel_layout->setSpacing(18);
    panel_layout->addWidget(lab_title);
    panel_layout->addSpacing(8);

    // QFormLayout 适合“左侧标签 + 右侧输入控件”的表单结构。
    QFormLayout *form_layout = new QFormLayout;
    form_layout->setSpacing(20);
    form_layout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    edit_user->setObjectName("loginUserName");
    edit_pwd->setObjectName("loginPassword");
    edit_user->setPlaceholderText("请输入用户名");
    edit_pwd->setPlaceholderText("请输入密码");
    // Password 模式只改变界面显示，不等于对密码进行了加密。
    edit_pwd->setEchoMode(QLineEdit::Password);
    form_layout->addRow("用户名：", edit_user);

    QHBoxLayout *password_layout = new QHBoxLayout;
    QPushButton *btn_show_password = new QPushButton("显示密码");
    btn_show_password->setCheckable(true);
    btn_show_password->setMinimumWidth(92);
    password_layout->addWidget(edit_pwd, 1);
    password_layout->addWidget(btn_show_password);
    form_layout->addRow("密  码：", password_layout);

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
        "登录后查看画面和管理设置。后台监控按通道配置持续运行；退出程序将停止监控。"
    );
    lab_tip->setObjectName("tipLabel");
    lab_tip->setWordWrap(true);
    panel_layout->addWidget(lab_tip);

    QHBoxLayout *button_layout = new QHBoxLayout;
    QPushButton *btn_login = new QPushButton("登录");
    QPushButton *btn_reg = new QPushButton("注册");
    btn_reg->setObjectName("registerButton");
    QPushButton *btn_cancel = new QPushButton("退出程序");
    btn_cancel->setToolTip("关闭整个程序并停止后台监控；保持登录窗口打开可持续监控");
    btn_login->setObjectName("primaryButton");

    button_layout->addWidget(btn_login);
    button_layout->addWidget(btn_reg);
    button_layout->addWidget(btn_cancel);
    panel_layout->addLayout(button_layout);

    main_layout->addStretch();


    main_layout->addWidget(form_panel, 0, Qt::AlignHCenter);
    main_layout->addStretch();

    AppTheme::applyLogin(this);

    // 一个槽可以接收多个信号：点击登录按钮或在输入框按回车都会执行 login()。
    connect(btn_login, SIGNAL(clicked()),
            this, SLOT(login()));
    connect(edit_pwd, SIGNAL(returnPressed()),
            this, SLOT(login()));
    connect(edit_vcode, SIGNAL(returnPressed()),
            this, SLOT(login()));

    connect(btn_cancel, SIGNAL(clicked()),
            this, SLOT(cancel()));

    connect(btn_show_password, &QPushButton::toggled,
            this, [this, btn_show_password](bool checked) {
        edit_pwd->setEchoMode(
            checked ? QLineEdit::Normal : QLineEdit::Password
        );
        btn_show_password->setText(checked ? "隐藏密码" : "显示密码");
    });

    // 函数指针 + Lambda 是现代 connect 写法。
    // this 是 Lambda 的上下文对象，窗口销毁后该连接会自动断开。
    connect(btn_reg, &QPushButton::clicked,
            this, [this, lab_tip]() {
        QDialog dialog(this);
        dialog.setObjectName("registrationDialog");
        dialog.setWindowTitle("注册管理员账号");
        dialog.setMinimumWidth(480);
        AppTheme::applyLogin(&dialog);
        auto *layout = new QVBoxLayout(&dialog);
        auto *tip = new QLabel("账号：6-10 位字母、数字或下划线\n密码：6-8 位，不能包含空白字符", &dialog);
        tip->setWordWrap(true);
        layout->addWidget(tip);
        auto *form = new QFormLayout;
        auto *name = new QLineEdit(edit_user->text().trimmed(), &dialog);
        auto *password = new QLineEdit(&dialog);
        auto *confirmation = new QLineEdit(&dialog);
        name->setObjectName("registrationName");
        password->setObjectName("registrationPassword");
        confirmation->setObjectName("registrationConfirmation");
        password->setEchoMode(QLineEdit::Password);
        confirmation->setEchoMode(QLineEdit::Password);
        form->addRow("用户名", name);
        form->addRow("密码", password);
        form->addRow("确认密码", confirmation);
        layout->addLayout(form);
        auto *message = new QLabel(&dialog);
        message->setObjectName("registrationMessage");
        message->setTextFormat(Qt::PlainText);
        message->setWordWrap(true);
        layout->addWidget(message);
        auto *buttons = new QDialogButtonBox(&dialog);
        auto *submit = buttons->addButton("注册", QDialogButtonBox::AcceptRole);
        submit->setObjectName("registrationSubmit");
        submit->setDefault(true);
        buttons->addButton("返回登录", QDialogButtonBox::RejectRole);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
            const auto result = user_service.registerUser(name->text(), password->text(), confirmation->text());
            message->setText(result.message);
            if (result.status != RegistrationResult::Success) return;
            edit_user->setText(name->text().trimmed());
            edit_pwd->clear();
            edit_vcode->clear();
            pic_vcode->updateVerificationCode();
            lab_tip->setText(result.message);
            dialog.accept();
        });
        name->setFocus();
        if (dialog.exec() == QDialog::Accepted) edit_pwd->setFocus();
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
    if (!AuthPolicy::isUserNameValid(user_name)) {
        QMessageBox::warning(
            this,
            "账号格式错误",
            "管理员账号须为 6-10 位字母、数字或下划线。"
        );
        edit_user->setFocus();
        return;
    }
    if (!AuthPolicy::isPasswordValid(password)) {
        QMessageBox::warning(
            this,
            "密码格式错误",
            "密码须为 6-8 位且不能包含空白字符。"
        );
        edit_pwd->setFocus();
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

    if (result.status == LoginResult::Locked) {
        const int minutes = (result.lockSeconds + 59) / 60;
        QMessageBox::warning(
            this,
            "账号已锁定",
            QString("连续 3 次登录失败，该账号已锁定。请在约 %1 分钟后重试。")
                .arg(minutes)
        );
        edit_pwd->clear();
        edit_vcode->clear();
        pic_vcode->updateVerificationCode();
        edit_pwd->setFocus();
        return;
    }

    if (result.status == LoginResult::InvalidCredentials) {
        QMessageBox::warning(
            this,
            "登录失败",
            QString("用户名或密码错误，还可尝试 %1 次。")
                .arg(result.remainingAttempts)
        );
        edit_pwd->clear();
        edit_vcode->clear();
        pic_vcode->updateVerificationCode();
        edit_pwd->setFocus();
        return;
    }

    if (result.status == LoginResult::InvalidUserNameFormat
            || result.status == LoginResult::InvalidPasswordFormat) {
        QMessageBox::warning(this, "登录失败", "账号或密码格式不符合要求。");
        return;
    }

    QMessageBox::information(this, "登录成功", "验证通过，正在返回主界面。");
    emit login_success(result.userId, result.userName);

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
    // 标题栏关闭按钮与“退出程序”按钮采用相同的状态通知。
    emit login_cancelled();

    // 接受关闭事件，让本次关闭继续执行。
    event->accept();
}
