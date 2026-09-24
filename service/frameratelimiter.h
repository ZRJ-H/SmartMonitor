// 【阅读导引】
// 按通道限帧：根据传入的单调毫秒时间决定本次是否放行，不创建线程或定时器。

#ifndef FRAMERATELIMITER_H
#define FRAMERATELIMITER_H

#include <QtGlobal>

class FrameRateLimiter
{
public:
    explicit FrameRateLimiter(int maximumFps = 15);
    bool shouldPass(int channelIndex, qint64 timestampMs);
    void reset(int channelIndex = -1);

private:
    qint64 interval_ms_;
    qint64 next_due_ms_[4];
};

#endif // FRAMERATELIMITER_H
