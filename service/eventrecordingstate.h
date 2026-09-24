// 【阅读导引】
// 事件采样状态：拒绝覆盖活跃事件，在收到满足时间阈值的帧时最多采样三次。
// 时间由调用者传入，因此可直接测试边界；此类本身没有时钟、磁盘或数据库。

#ifndef EVENTRECORDINGSTATE_H
#define EVENTRECORDINGSTATE_H
#include <QString>
// 纯事件时序：重复触发不延长当前事件；每个采样时点只消费一次。
class EventRecordingState {
public:
    // 活跃事件期间拒绝重复开启，防止重复检测无限延长同一事件。
    bool begin(const QString &key) {
        if(active() || key.trimmed().isEmpty())return false;
        key_=key;samples_=0;return true;
    }
    bool active() const {return !key_.isEmpty();}
    QString key() const {return key_;}
    // 第 1/2/3 次采样阈值分别为 0/1000/2000 毫秒；没有新帧时不会凭空产生图片。
    bool takeSample(qint64 elapsedMs) {
        if(!active()||samples_>=3||elapsedMs<samples_*1000)return false;
        ++samples_;return true;
    }
    bool shouldFinish(qint64 elapsedMs) const {return active()&&elapsedMs>=60000;}
    void reset(){key_.clear();samples_=0;}
private:
    QString key_;
    int samples_=0;
};
#endif
