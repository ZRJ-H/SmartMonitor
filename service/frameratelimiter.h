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
