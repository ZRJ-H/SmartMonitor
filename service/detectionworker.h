// YOLO 运算只在本对象所属的工作线程执行；算法与原采集任务保持一致。
// 无 Q_OBJECT：通过 invokeMethod 的函数对象排队调用，不使用字符串槽名。
#ifndef DETECTIONWORKER_H
#define DETECTIONWORKER_H
#include <QObject>
#include <QString>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
struct DetectionResult {
    bool found = false;
    double confidence = 0;
    QString error;
    std::vector<cv::Rect> boxes;
};
class DetectionWorker : public QObject {
public:
    QString configure(bool enabled, const QString &path, double threshold) {
        net = cv::dnn::Net(); confidence = threshold;
        if (!enabled) return {};
        try {
            net = cv::dnn::readNetFromONNX(path.toStdString());
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        } catch (const cv::Exception &error) { return QString::fromUtf8(error.what()); }
        return {};
    }
    DetectionResult detect(const cv::Mat &frame) {
        DetectionResult result;
        if(net.empty()) return result;
        bool foundPerson = false;
        double bestConfidence = 0.0;
        try {
            cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
                cv::Size(640, 640), cv::Scalar(), true, false);
            net.setInput(blob);
            cv::Mat output = net.forward();
            if (output.dims == 3) {
                cv::Mat rows(output.size[1], output.size[2], CV_32F,
                             output.ptr<float>());
                if (rows.rows < rows.cols) cv::transpose(rows, rows);
                const bool yolo8 = rows.cols == 84;
                for (int i = 0; i < rows.rows; ++i) {
                    const float *data = rows.ptr<float>(i);
                    const double score = yolo8 ? data[4] : data[4] * data[5];
                    if (score < confidence) continue;
                    const double xScale = frame.cols / 640.0;
                    const double yScale = frame.rows / 640.0;
                    int left = qMax(0, qRound((data[0] - data[2] / 2.0) * xScale));
                    int top = qMax(0, qRound((data[1] - data[3] / 2.0) * yScale));
                    int width = qMin(qRound(data[2] * xScale), frame.cols - left);
                    int height = qMin(qRound(data[3] * yScale), frame.rows - top);
                    if (width > 0 && height > 0)
                        result.boxes.push_back(cv::Rect(left, top, width, height));
                    foundPerson = true;
                    bestConfidence = qMax(bestConfidence, score);
                }
            }
        } catch (const cv::Exception &error) {
            result.error = QString::fromUtf8(error.what());
        }
        result.found = foundPerson;
        result.confidence = bestConfidence;
        return result;
    }
private:
    cv::dnn::Net net;
    double confidence = 0.5;
};
#endif
