// 【阅读导引】
// 图片数据契约：FeatureImage 携带查询/保存结果，ImageFilter 携带筛选条件。
// 事件 ID、录像 ID、图片 ID 分属不同表，不能因为都是整数就互相替代。

#ifndef FEATUREIMAGE_H
#define FEATUREIMAGE_H

#include <QDateTime>
#include <QString>

struct FeatureImage
{
    qint64 id = 0;
    QString imageName;
    QString imagePath;
    int channelId = 0;
    QDateTime captureTime;
    QString captureSource;
    qint64 exceptionId = 0;
    qint64 videoId = 0;
    qint64 videoOffsetMs = 0; // 相对关联录像起点的毫秒数，用于图片跳转到回放位置。
};

// 无效日期、通道 0、空来源、事件 ID 0 表示不施加对应筛选条件。
struct ImageFilter
{
    QDateTime from;
    QDateTime to;
    int channelId = 0;
    QString captureSource;
    qint64 exceptionId = 0;
};

#endif // FEATUREIMAGE_H
