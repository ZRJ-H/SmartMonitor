#ifndef EVENTRECORDINGSTATE_H
#define EVENTRECORDINGSTATE_H
#include <QString>
// 纯事件时序：重复触发不延长当前事件；每个采样时点只消费一次。
class EventRecordingState {
public:
    bool begin(const QString &key) {
        if(active() || key.trimmed().isEmpty())return false;
        key_=key;samples_=0;return true;
    }
    bool active() const {return !key_.isEmpty();}
    QString key() const {return key_;}
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
