#include <QtTest>
#include <QSemaphore>
#include <atomic>
#include <thread>
#include <vector>

#include "../service/latestframebuffer.h"

class FrameBufferTests : public QObject
{
    Q_OBJECT
private slots:
    void emptyInputDoesNotScheduleCallback();
    void burstKeepsNewestFrameAndOneCallback();
    void takeReleasesMailboxAndAllowsNextCallback();
    void clearReleasesFrameAndResetsStoppedMailbox();
    void concurrentProducerAndConsumerDeliverFinalFrame();
};

void FrameBufferTests::emptyInputDoesNotScheduleCallback()
{
    LatestFrameBuffer mailbox;
    QVERIFY(mailbox.take().empty());
    QVERIFY(!mailbox.publish(cv::Mat()));
    const cv::Mat frame(1, 1, CV_32S, cv::Scalar(42));
    QVERIFY(mailbox.publish(frame));
    QVERIFY(!mailbox.publish(cv::Mat()));
    QCOMPARE(mailbox.take().at<int>(0, 0), 42);
    QVERIFY(mailbox.take().empty());
}

void FrameBufferTests::burstKeepsNewestFrameAndOneCallback()
{
    LatestFrameBuffer mailbox;
    std::vector<cv::Mat> inputs;
    inputs.reserve(1000);
    int callbacks = 0;
    for (int value = 0; value < 1000; ++value) {
        inputs.emplace_back(8, 8, CV_32S, cv::Scalar(value));
        if (mailbox.publish(inputs.back())) ++callbacks;
    }

    QCOMPARE(callbacks, 1);
    // Inputs retain one reference each. The mailbox may retain only the newest
    // buffer: replacing stale frames must release their pixel allocations.
    for (size_t index = 0; index + 1 < inputs.size(); ++index)
        QCOMPARE(inputs[index].u->refcount, 1);
    QCOMPARE(inputs.back().u->refcount, 2);

    const cv::Mat newest = mailbox.take();
    QCOMPARE(newest.at<int>(0, 0), 999);
    QVERIFY(newest.data == inputs.back().data); // No full-frame copy.
    QCOMPARE(inputs.back().u->refcount, 2); // Input and returned consumer only.
    QVERIFY(mailbox.take().empty());
}

void FrameBufferTests::takeReleasesMailboxAndAllowsNextCallback()
{
    LatestFrameBuffer mailbox;
    const cv::Mat first(1, 1, CV_32S, cv::Scalar(1));
    const cv::Mat second(1, 1, CV_32S, cv::Scalar(2));
    const cv::Mat third(1, 1, CV_32S, cv::Scalar(3));

    QVERIFY(mailbox.publish(first));
    const cv::Mat consumed = mailbox.take();
    QVERIFY(mailbox.publish(second));
    QVERIFY(!mailbox.publish(third));
    QCOMPARE(consumed.at<int>(0, 0), 1);
    QCOMPARE(mailbox.take().at<int>(0, 0), 3);
    QVERIFY(mailbox.publish(first));
    QCOMPARE(mailbox.take().at<int>(0, 0), 1);
}

void FrameBufferTests::clearReleasesFrameAndResetsStoppedMailbox()
{
    LatestFrameBuffer mailbox;
    const cv::Mat frame(1, 1, CV_32S, cv::Scalar(17));
    QVERIFY(mailbox.publish(frame));
    QCOMPARE(frame.u->refcount, 2);
    // No producer or consumer is running while clear() is used.
    mailbox.clear();
    QCOMPARE(frame.u->refcount, 1);
    QVERIFY(mailbox.take().empty());
    mailbox.clear();
    QVERIFY(mailbox.publish(frame));
    QCOMPARE(mailbox.take().at<int>(0, 0), 17);
}

void FrameBufferTests::concurrentProducerAndConsumerDeliverFinalFrame()
{
    LatestFrameBuffer mailbox;
    QSemaphore wakeups;
    constexpr int frameCount = 20000;
    std::atomic<bool> producerDone{false};
    std::atomic<bool> invalidOrder{false};
    std::atomic<bool> timedOut{false};
    std::atomic<int> latestTaken{0};
    std::atomic<int> callbacks{0};

    std::thread consumer([&] {
        int previous = 0;
        for (;;) {
            if (!wakeups.tryAcquire(1, 10000)) {
                timedOut = true;
                return;
            }
            const cv::Mat frame = mailbox.take();
            if (!frame.empty()) {
                const int value = frame.at<int>(0, 0);
                if (value <= previous || value > frameCount) invalidOrder = true;
                previous = value;
                latestTaken = value;
            }
            if (producerDone && previous == frameCount) return;
        }
    });

    std::thread producer([&] {
        for (int value = 1; value <= frameCount; ++value) {
            const cv::Mat frame(4, 4, CV_32S, cv::Scalar(value));
            if (mailbox.publish(frame)) {
                ++callbacks;
                wakeups.release();
            }
            if (value % 97 == 0) std::this_thread::yield();
        }
        producerDone = true;
        // A separate shutdown wake-up lets the consumer recheck producerDone
        // even if it consumed the last frame just before the flag was stored.
        wakeups.release();
    });
    producer.join();
    consumer.join();

    QVERIFY(!timedOut.load());
    QVERIFY(!invalidOrder.load());
    QCOMPARE(latestTaken.load(), frameCount);
    QVERIFY(callbacks.load() >= 1);
    QVERIFY(callbacks.load() <= frameCount);
    QVERIFY(mailbox.take().empty());
    const cv::Mat next(1, 1, CV_32S, cv::Scalar(frameCount + 1));
    QVERIFY(mailbox.publish(next));
    QCOMPARE(mailbox.take().at<int>(0, 0), frameCount + 1);
}

QTEST_GUILESS_MAIN(FrameBufferTests)
#include "tst_frame_buffer.moc"
