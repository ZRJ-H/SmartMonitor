// YOLO 运算只在本对象所属的工作线程执行；算法与原采集任务保持一致。
// 无 Q_OBJECT：通过 invokeMethod 的函数对象排队调用，不使用字符串槽名。
#ifndef DETECTIONWORKER_H
#define DETECTIONWORKER_H
#include <QObject>
#include <QString>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
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
    static std::shared_ptr<SharedDetectionEngine> acquire(const QString &path){
        static std::mutex cacheMutex;
        static std::map<QString,std::weak_ptr<SharedDetectionEngine>> cache;
        std::lock_guard<std::mutex> lock(cacheMutex);
        const QFileInfo info(path);
        const QString mode=qEnvironmentVariable("SMART_MONITOR_DETECTION_BACKEND","auto").toLower();
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
    QString configure(bool enabled, const QString &path, double threshold) {
        engine.reset(); confidence = threshold;
        if (!enabled) return {};
        try { engine=SharedDetectionEngine::acquire(path); }
        catch(const std::exception &error){return QString::fromUtf8(error.what());}
        return {};
    }
    QString backend() const {return engine?engine->description():QString("未启用");}
    DetectionResult detect(const cv::Mat &frame) {
        DetectionResult result;
        if(!engine) return result;
        bool foundPerson = false;
        double bestConfidence = 0.0;
        try {
            cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
                cv::Size(640, 640), cv::Scalar(), true, false);
            cv::Mat output = engine->forward(blob);
            if (output.dims == 3) {
                cv::Mat rows(output.size[1], output.size[2], CV_32F,
                             output.ptr<float>());
                if (rows.rows < rows.cols) cv::transpose(rows, rows);
                if (rows.cols != 84 && rows.cols != 85) throw std::runtime_error("Unsupported YOLO output layout");
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
        } catch (const std::exception &error) {
            result.error = QString::fromUtf8(error.what());
        }
        result.backend=engine->description();
        result.found = foundPerson;
        result.confidence = bestConfidence;
        return result;
    }
private:
    std::shared_ptr<SharedDetectionEngine> engine;
    double confidence = 0.5;
};
#endif
