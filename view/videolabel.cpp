// 【阅读导引】
// 预览标签：覆盖尺寸提示，避免上一帧像素尺寸参与布局后导致窗口不断膨胀。

#include "videolabel.h"

#include <QSizePolicy>

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
VideoLabel::VideoLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

// 返回不依赖当前像素尺寸的首选大小，由外层布局分配显示空间。
QSize VideoLabel::sizeHint() const
{
    return QSize(0, 0);
}

// 不让已显示的大图形成新的最小尺寸，避免布局撑大后无法缩回。
QSize VideoLabel::minimumSizeHint() const
{
    return QSize(0, 0);
}
