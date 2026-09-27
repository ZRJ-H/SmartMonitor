#ifndef SOURCEEVENTROW_H
#define SOURCEEVENTROW_H
#include <QList>
#include <QMetaType>
#include <QString>
struct SourceEventRow {
    QString key, path, image, channels, asset, analysis, analyzedAt;
    qint64 startMs = 0, endMs = 0;
    double confidence = 0;
};
Q_DECLARE_METATYPE(SourceEventRow)
Q_DECLARE_METATYPE(QList<SourceEventRow>)
#endif
