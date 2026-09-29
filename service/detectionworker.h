// YOLO 运算只在本对象所属的工作线程执行；CPU/GPU 共用有效框筛选与 NMS 后处理。
// 无 Q_OBJECT：通过 invokeMethod 的函数对象排队调用，不使用字符串槽名。
#ifndef DETECTIONWORKER_H
#define DETECTIONWORKER_H
#include <QObject>
#include <QString>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <map>
#include <mutex>
#include <condition_variable>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include "ortgpusession.h"
// One shared model per model revision/backend. Ticket order prevents a channel
// from monopolizing inference. UI/capture never waits on this lock.
struct SharedDetectionEngine {
    cv::dnn::Net cpu;
    std::unique_ptr<OrtGpuSession> gpu;
    QString model, mode, backend, fallback;
    std::mutex mutex;
    std::condition_variable ready;
    quint64 next=0, serving=0;
    struct Turn {
        SharedDetectionEngine &owner;
        std::unique_lock<std::mutex> lock;
        Turn(SharedDetectionEngine &e):owner(e),lock(e.mutex){auto ticket=e.next++;e.ready.wait(lock,[&]{return ticket==e.serving;});lock.unlock();}
        ~Turn(){lock.lock();++owner.serving;lock.unlock();owner.ready.notify_all();}
    };
    void loadCpu(){cpu=cv::dnn::readNetFromONNX(model.toStdString());cpu.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);cpu.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);backend="CPU / OpenCV";}
    static std::shared_ptr<SharedDetectionEngine> acquire(const QString &path, const QString &requestedMode = {}){
        static std::mutex cacheMutex;
        static std::map<QString,std::weak_ptr<SharedDetectionEngine>> cache;
        std::lock_guard<std::mutex> lock(cacheMutex);
        const QFileInfo info(path);
        const QString mode=(requestedMode.isEmpty() ? qEnvironmentVariable("SMART_MONITOR_DETECTION_BACKEND","cpu") : requestedMode).toLower();
        if(mode!="auto"&&mode!="cpu"&&mode!="directml")throw std::runtime_error("Unknown detection backend");
        const QString directory=qEnvironmentVariable("SMART_MONITOR_GPU_DIR",QCoreApplication::applicationDirPath()+"/gpu");
        const QString key=info.canonicalFilePath()+QString::number(info.size())+QString::number(info.lastModified().toMSecsSinceEpoch())+mode+directory;
        if(auto existing=cache[key].lock())return existing;
        auto engine=std::make_shared<SharedDetectionEngine>();engine->model=path;engine->mode=mode;
        if(mode!="cpu"){
            try{
                engine->gpu=std::make_unique<OrtGpuSession>(directory.toStdWString(),info.absoluteFilePath().toStdWString());
                // Force actual execution before declaring GPU ready.
                cv::Mat warm=cv::dnn::blobFromImage(cv::Mat::zeros(640,640,CV_8UC3),1./255,cv::Size(640,640),cv::Scalar(),true,false);
                engine->gpu->run(warm);
                engine->backend="GPU / DirectML / "+QString::fromStdWString(engine->gpu->device());
            }catch(const std::exception &e){engine->gpu.reset();if(mode=="directml")throw;engine->fallback=QString::fromUtf8(e.what());}
        }
        if(!engine->gpu)engine->loadCpu();
        qInfo().noquote()<<"Inference backend:"<<engine->backend<<engine->fallback;
        cache[key]=engine;return engine;
    }
    cv::Mat forward(const cv::Mat &blob){
        Turn turn(*this);
        if(gpu){try{return gpu->run(blob);}catch(const std::exception &e){if(mode=="directml")throw;
            fallback=QString::fromUtf8(e.what());gpu.reset();loadCpu();qWarning().noquote()<<"GPU failed, CPU fallback:"<<fallback;}}
        cpu.setInput(blob);return cpu.forward().clone();
    }
    QString description(){Turn turn(*this);return backend+(fallback.isEmpty()?QString():"（降级原因："+fallback+"）");}
};
struct DetectionResult {
    bool found = false;
    double confidence = 0;
    QString error;
    QString backend;
    std::vector<cv::Rect> boxes;
};
class DetectionWorker : public QObject {
public:
    QString configure(bool enabled, const QString &path, double threshold, const QString &mode = {}) {
        if (!enabled) { engine.reset(); confidence = threshold; return {}; }
        // Build and warm the replacement before releasing the previous session.
        try { auto replacement=SharedDetectionEngine::acquire(path, mode); engine=std::move(replacement); confidence=threshold; }
        catch(const std::exception &error){return QString::fromUtf8(error.what());}
        return {};
    }
    QString backend() const {return engine?engine->description():QString("未启用");}
    DetectionResult detect(const cv::Mat &frame) {
        DetectionResult result;
        if (!engine) return result;
        try {
            const cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
                cv::Size(640, 640), cv::Scalar(), true, false);
            result = postprocess(engine->forward(blob), frame.size(), confidence);
        } catch (const std::exception &error) {
            result.error = QString::fromUtf8(error.what());
        }
        result.backend = engine->description();
        return result;
    }

    // 版本进入文件分析身份，旧版未去重的样本保留为历史，不作为新结果复用。
    static QString analysisProfile() { return QStringLiteral("stretch640-person-nms045-v2"); }

    // 仅支持 COCO 80 类原始 YOLOv8/v5 输出；CPU/GPU 共用坐标裁剪及 NMS。
    static DetectionResult postprocess(cv::Mat output, const cv::Size &frameSize,
                                      double threshold) {
        DetectionResult result;
        try {
            if (frameSize.width <= 0 || frameSize.height <= 0
                || !std::isfinite(threshold) || threshold <= 0 || threshold > 1)
                throw std::runtime_error("Invalid detection size or confidence threshold");
            if (output.empty() || output.dims != 3 || output.size[0] != 1
                || output.type() != CV_32F)
                throw std::runtime_error("Unsupported YOLO output tensor; expected FP32 batch 1");
            const auto attributes = [](int n) { return n == 84 || n == 85; };
            const bool channelsFirst = attributes(output.size[1]);
            const bool channelsLast = attributes(output.size[2]);
            if (channelsFirst == channelsLast)
                throw std::runtime_error("Unsupported or ambiguous YOLO output layout");
            if (!output.isContinuous()) output = output.clone();
            cv::Mat rows = output.reshape(1, output.size[1]);
            if (channelsFirst) cv::transpose(rows, rows);
            const bool yolo8 = rows.cols == 84;
            const double xScale = frameSize.width / 640.0;
            const double yScale = frameSize.height / 640.0;
            std::vector<cv::Rect> boxes;
            std::vector<float> scores;
            for (int i = 0; i < rows.rows; ++i) {
                const float *p = rows.ptr<float>(i);
                const double personScore = yolo8 ? p[4] : p[5];
                const double objectness = yolo8 ? 1.0 : p[4];
                if (!std::isfinite(personScore) || !std::isfinite(objectness)
                    || personScore < 0 || personScore > 1 || objectness < 0 || objectness > 1)
                    continue;
                const double score = personScore * objectness;
                if (score < threshold) continue;
                if (!std::isfinite(p[0]) || !std::isfinite(p[1])
                    || !std::isfinite(p[2]) || !std::isfinite(p[3]) || p[2] <= 0 || p[3] <= 0)
                    continue;
                // 先裁剪左右/上下端点再求宽高，防止左上越界时框被错误扩大。
                // 在浮点范围内限幅后转整数，避免异常模型坐标造成整数溢出。
                const int left = qRound(std::clamp((double(p[0])-p[2]/2.0)*xScale, 0.0, double(frameSize.width)));
                const int top = qRound(std::clamp((double(p[1])-p[3]/2.0)*yScale, 0.0, double(frameSize.height)));
                const int right = qRound(std::clamp((double(p[0])+p[2]/2.0)*xScale, 0.0, double(frameSize.width)));
                const int bottom = qRound(std::clamp((double(p[1])+p[3]/2.0)*yScale, 0.0, double(frameSize.height)));
                if (right <= left || bottom <= top) continue;
                boxes.emplace_back(left, top, right-left, bottom-top);
                scores.push_back(float(score));
            }
            std::vector<int> kept;
            // 已按用户阈值筛选；这里以 0 作为分数门限，保留恰好等于用户阈值的框。
            if (!boxes.empty()) cv::dnn::NMSBoxes(boxes, scores, 0.0f, 0.45f, kept);
            for (int index : kept) {
                result.boxes.push_back(boxes[index]);
                result.confidence = std::max(result.confidence, double(scores[index]));
            }
            result.found = !result.boxes.empty();
        } catch (const std::exception &error) {
            result = DetectionResult{};
            result.error = QString::fromUtf8(error.what());
        }
        return result;
    }
private:
    std::shared_ptr<SharedDetectionEngine> engine;
    double confidence = 0.5;
};
#endif
