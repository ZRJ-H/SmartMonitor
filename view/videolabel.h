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
