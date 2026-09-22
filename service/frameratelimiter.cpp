#include "frameratelimiter.h"

FrameRateLimiter::FrameRateLimiter(int maximumFps)
    : interval_ms_((1000 + qMax(1, maximumFps) - 1) / qMax(1, maximumFps))
{
    reset();
}

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

void FrameRateLimiter::reset(int channelIndex)
{
    if (channelIndex >= 0 && channelIndex < 4) {
        next_due_ms_[channelIndex] = -1;
        return;
    }
    for (qint64 &nextDue : next_due_ms_) nextDue = -1;
}
