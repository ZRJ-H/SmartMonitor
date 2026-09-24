// 【阅读导引】
// 按通道限帧：根据传入的单调毫秒时间决定本次是否放行，不创建线程或定时器。

#include "frameratelimiter.h"

// 构造对象并初始化所需成员；冒号后的初始化列表先于函数体执行。
FrameRateLimiter::FrameRateLimiter(int maximumFps)
    : interval_ms_((1000 + qMax(1, maximumFps) - 1) / qMax(1, maximumFps))
{
    reset();
}

// 检查该通道时间是否到期；放行后推进截止时间，落后过多时跳到当前节拍。
bool FrameRateLimiter::shouldPass(int channelIndex, qint64 timestampMs)
{
    if (channelIndex < 0 || channelIndex >= 4) return false;
    qint64 &nextDue = next_due_ms_[channelIndex];
    if (nextDue < 0 || timestampMs < nextDue - interval_ms_) {
        nextDue = timestampMs + interval_ms_;
        return true;
    }
    if (timestampMs < nextDue) return false;
    nextDue += interval_ms_;
    if (nextDue <= timestampMs) nextDue = timestampMs + interval_ms_;
    return true;
}

// 重置指定通道；默认负下标表示重置全部四路，让下一帧可立即放行。
void FrameRateLimiter::reset(int channelIndex)
{
    if (channelIndex >= 0 && channelIndex < 4) {
        next_due_ms_[channelIndex] = -1;
        return;
    }
    for (qint64 &nextDue : next_due_ms_) nextDue = -1;
}
