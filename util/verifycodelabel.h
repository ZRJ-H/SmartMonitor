#ifndef VERIFYCODELABEL_H
#define VERIFYCODELABEL_H

#include <QLabel>
#include <QString>

class QMouseEvent;
class QPaintEvent;

// 自定义验证码控件：继承 QLabel，自己生成字符、绘制图像并处理点击刷新。
class VerifyCodeLabel : public QLabel
{
    Q_OBJECT

public:
    explicit VerifyCodeLabel(QWidget *parent = nullptr);

    // 返回当前验证码。统一转成大写，便于登录时忽略大小写比较。
    QString getVerificationCode() const;

    // 生成一组新字符，并通知 Qt 重绘控件。
    void updateVerificationCode();

protected:
    // paintEvent() 决定控件如何绘制；mouseReleaseEvent() 处理鼠标点击。
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // 每一位验证码可以是数字、大写字母或小写字母。
    enum CharacterType
    {
        NumberFlag,
        UpperLetterFlag,
        LowerLetterFlag
    };

    // 此函数只生成验证码字符串，不负责绘制。
    void produceVerificationCode();

    // 验证码字符数和噪点数在对象构造后保持不变。
    const int letter_number;
    const int noise_point_number;
    QString verification_code;
};

#endif // VERIFYCODELABEL_H
