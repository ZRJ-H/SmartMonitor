// 声明一个最小的 Qt 信号槽教学窗口，用于演示按钮点击与自定义关闭槽的连接。
#ifndef MYWIDGET_H
#define MYWIDGET_H

#include <QWidget>

// 本类是“系统信号连接自定义槽”的最小案例。
class MyWidget : public QWidget
{
    // Q_OBJECT 让该类具有 Qt 元对象能力，自定义槽才能被 SLOT() 找到。
    Q_OBJECT

public:
    // 创建按钮和布局，并建立系统信号到 myClose() 的连接。
    explicit MyWidget(QWidget *parent = nullptr);

public slots:
    // 点击“关闭窗口”按钮后执行，用消息框询问用户是否确认关闭。
    void myClose();
};

#endif // MYWIDGET_H
