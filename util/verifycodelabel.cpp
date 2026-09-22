// 实现验证码字符随机生成、抗干扰绘制以及鼠标点击刷新。
#include "verifycodelabel.h"

#include <QColor>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QtGlobal>

VerifyCodeLabel::VerifyCodeLabel(QWidget *parent)
    : QLabel(parent),
      letter_number(4),
      noise_point_number(80)
{
    setFixedSize(100, 36);
    setCursor(Qt::PointingHandCursor);
    setToolTip("点击刷新验证码");

    // 对象创建后立刻生成第一组验证码。
    produceVerificationCode();
}

QString VerifyCodeLabel::getVerificationCode() const
{
    // 登录窗口使用不区分大小写的比较；这里转成大写让接口含义更明确。
    return verification_code.toUpper();
}

void VerifyCodeLabel::updateVerificationCode()
{
    produceVerificationCode();

    // update() 不会立刻手动调用 paintEvent()，而是向 Qt 请求一次合适时机的重绘。
    update();
}

void VerifyCodeLabel::produceVerificationCode()
{
    verification_code.clear();

    // 循环生成四位字符，每一位随机选择数字、大写字母或小写字母。
    for (int i = 0; i < letter_number; ++i) {
        const int flag = QRandomGenerator::global()->bounded(3);
        QChar character;

        switch (flag) {
        case NumberFlag:
            character = QChar('0' + QRandomGenerator::global()->bounded(10));
            break;
        case UpperLetterFlag:
            character = QChar('A' + QRandomGenerator::global()->bounded(26));
            break;
        case LowerLetterFlag:
            character = QChar('a' + QRandomGenerator::global()->bounded(26));
            break;
        default:
            // 正常情况下不会进入 default，它只是提供一个安全的备用值。
            character = QChar('0');
            break;
        }

        verification_code.append(character);
    }
}

void VerifyCodeLabel::paintEvent(QPaintEvent *event)
{
    // 本案例完全自行绘制 QLabel，不需要使用 event 参数。
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // 第一步：绘制验证码浅色背景。
    painter.fillRect(rect(), QColor(245, 245, 245));

    // 第二步：随机绘制噪点，增加验证码背景干扰。
    for (int i = 0; i < noise_point_number; ++i) {
        painter.setPen(QColor(
            QRandomGenerator::global()->bounded(180),
            QRandomGenerator::global()->bounded(180),
            QRandomGenerator::global()->bounded(180)
        ));
        painter.drawPoint(
            QRandomGenerator::global()->bounded(width()),
            QRandomGenerator::global()->bounded(height())
        );
    }

    // 每个字符占据相同宽度，四位字符正好铺满控件。
    const int character_width = width() / letter_number;

    // 第三步：逐个绘制字符，并随机改变颜色、字号和旋转角度。
    for (int i = 0; i < letter_number; ++i) {
        // save()/restore() 保证当前字符的旋转不会影响后面的字符。
        painter.save();
        painter.setPen(QColor(
            QRandomGenerator::global()->bounded(150),
            QRandomGenerator::global()->bounded(150),
            QRandomGenerator::global()->bounded(150)
        ));

        QFont font("Arial");
        font.setPointSize(15 + QRandomGenerator::global()->bounded(5));
        font.setBold(true);
        painter.setFont(font);

        const QRect character_rect(
            i * character_width,
            0,
            character_width,
            height()
        );

        // 把坐标原点移动到字符区域中心，再旋转 -15° 到 15°。
        painter.translate(character_rect.center());
        painter.rotate(QRandomGenerator::global()->bounded(31) - 15);

        const QRect draw_rect(
            -character_width / 2,
            -height() / 2,
            character_width,
            height()
        );

        painter.drawText(
            draw_rect,
            Qt::AlignCenter,
            QString(verification_code.at(i))
        );
        painter.restore();
    }

    // 最后绘制边框。-1 可以避免右边和下边超出控件有效像素范围。
    painter.setPen(QColor(150, 150, 150));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void VerifyCodeLabel::mouseReleaseEvent(QMouseEvent *event)
{
    // 只在释放鼠标左键时刷新验证码。
    if (event->button() == Qt::LeftButton) {
        updateVerificationCode();
    }

    // 调用基类实现，保留 QLabel 原有的鼠标事件处理链。
    QLabel::mouseReleaseEvent(event);
}
