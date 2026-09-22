// 声明可点击刷新的图形验证码控件，负责随机字符生成和自绘干扰背景。
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
    // 创建固定尺寸的验证码标签，并立即生成第一组验证码。
    explicit VerifyCodeLabel(QWidget *parent = nullptr);

    // 返回当前验证码。统一转成大写，便于登录时忽略大小写比较。
    QString getVerificationCode() const;

    // 生成一组新字符，并通知 Qt 重绘控件。
    void updateVerificationCode();

protected:
    // 重绘背景、噪点和随机颜色字符；event 仅满足 Qt 重写接口。
    void paintEvent(QPaintEvent *event) override;
    // 鼠标左键释放时刷新验证码，其他按键交回 QLabel 处理。
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
    const int letter_number;      // 每组验证码的字符数量。
    const int noise_point_number; // 绘制在背景上的随机噪点数量。
    QString verification_code;    // 当前显示的原始验证码字符串。
};

#endif // VERIFYCODELABEL_H
