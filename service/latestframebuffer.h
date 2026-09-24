// 【阅读导引】
// 跨线程最新帧信箱：只保留一帧和一个待消费通知，避免消费者落后时无限排队。
// 这是有界、可覆盖的缓存，不是保证每帧都被保存的队列；像素在发布后必须只读。

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
    // 返回 true 表示需要投递一次消费通知；false 也可能表示帧已更新、已有通知等待处理。
    bool publish(const cv::Mat &immutableFrame)
    {
        if (immutableFrame.empty()) return false;
        // RAII 锁：构造时加锁，离开作用域（包括提前 return）时自动解锁。
        QMutexLocker locker(&mutex_);
        // Mat 赋值共享像素引用；覆盖旧引用即可保持内存有界，发布后不能修改这些像素。
        latest_ = immutableFrame;
        if (pending_) return false;
        pending_ = true;
        return true;
    }

    // 消费时才取最新帧。std::move 转移 Mat 的持有权，不复制整张图像。
    cv::Mat take()
    {
        QMutexLocker locker(&mutex_);
        cv::Mat frame = std::move(latest_);
        pending_ = false;
        return frame;
    }

    // Call only after producers and queued consumers have stopped. Resetting a
    // pending wake-up while a callback is still queued breaks the wake-up bound.
    // 必须先停止生产者及排队的消费者，否则重置 pending_ 会破坏“最多一个通知”的约束。
    void clear()
    {
        QMutexLocker locker(&mutex_);
        latest_.release();
        pending_ = false;
    }

private:
    QMutex mutex_;
    cv::Mat latest_;
    bool pending_ = false; // 是否已经安排了尚未取帧的消费回调。
};

#endif // LATESTFRAMEBUFFER_H
