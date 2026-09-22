// 实现信号槽教学窗口：按钮触发确认对话框，用户确认后关闭窗口。
#include "mywidget.h"

#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

MyWidget::MyWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("系统信号与系统槽");
    resize(300, 400);

    // 布局对象负责自动安排按钮的位置和大小。
    // 传给 setLayout() 后，布局归当前窗口管理，不需要手动 delete。
    QVBoxLayout *main_layout = new QVBoxLayout;
    setLayout(main_layout);

    // btn_close 加入窗口布局后，会成为窗口的子对象并随窗口自动释放。
    QPushButton *btn_close = new QPushButton("关闭窗口");
    main_layout->addWidget(btn_close);

    // QPushButton::clicked() 是 Qt 已经提供的系统信号。
    // MyWidget::myClose() 是我们自己编写的槽函数。
    // connect() 建立连接后，用户每次点击按钮都会自动调用 myClose()。
    connect(btn_close, SIGNAL(clicked()),
            this, SLOT(myClose()));
}

void MyWidget::myClose()
{
    // warning() 会阻塞在这里等待用户选择 Yes 或 No，并返回所选按钮。
    const int result = QMessageBox::warning(
        this,
        "关闭窗口提示",
        "是否确认关闭？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    // 调试输出可以帮助观察 QMessageBox 的返回值。
    qDebug() << "result =" << result;

    // 只有用户选择 Yes 时才关闭窗口；选择 No 时槽函数直接结束。
    if (result == QMessageBox::Yes) {
        close();
    }
}
