// 【阅读导引】
// 预览标签：覆盖尺寸提示，避免上一帧像素尺寸参与布局后导致窗口不断膨胀。

#ifndef VIDEOLABEL_H
#define VIDEOLABEL_H

#include <QLabel>

class VideoLabel : public QLabel
{
public:
    explicit VideoLabel(const QString &text = QString(), QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
};

#endif // VIDEOLABEL_H
