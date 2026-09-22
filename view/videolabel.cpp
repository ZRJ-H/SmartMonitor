#include "videolabel.h"

#include <QSizePolicy>

VideoLabel::VideoLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

QSize VideoLabel::sizeHint() const
{
    return QSize(0, 0);
}

QSize VideoLabel::minimumSizeHint() const
{
    return QSize(0, 0);
}
