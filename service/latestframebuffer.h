#ifndef LATESTFRAMEBUFFER_H
#define LATESTFRAMEBUFFER_H

#include <QMutex>
#include <QMutexLocker>
#include <opencv2/core/mat.hpp>
#include <utility>

// A bounded mailbox for one producer/consumer route. Only the newest frame is
// retained, and at most one consumer wake-up needs to be queued at a time.
//
// Frames are shared by reference, not cloned: after publish(), the producer
// MUST NOT modify their pixels (including through another cv::Mat alias).
// Consumers must likewise treat the returned pixels as immutable. Clone before
// editing. MonitorTask's frameReady signal already supplies an immutable clone.
// Object lifetime must outlive both the producers and queued consumer callbacks.
class LatestFrameBuffer
{
public:
    // Returns true only when the caller must queue a consumer callback. Do not
    // queue the frame itself: the callback calls take() to obtain the newest one.
    bool publish(const cv::Mat &immutableFrame)
    {
        if (immutableFrame.empty()) return false;
        QMutexLocker locker(&mutex_);
        latest_ = immutableFrame;
        if (pending_) return false;
        pending_ = true;
        return true;
    }

    cv::Mat take()
    {
        QMutexLocker locker(&mutex_);
        cv::Mat frame = std::move(latest_);
        pending_ = false;
        return frame;
    }

    // Call only after producers and queued consumers have stopped. Resetting a
    // pending wake-up while a callback is still queued breaks the wake-up bound.
    void clear()
    {
        QMutexLocker locker(&mutex_);
        latest_.release();
        pending_ = false;
    }

private:
    QMutex mutex_;
    cv::Mat latest_;
    bool pending_ = false;
};

#endif // LATESTFRAMEBUFFER_H
