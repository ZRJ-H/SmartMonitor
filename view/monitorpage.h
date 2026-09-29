#ifndef MONITORPAGE_H
#define MONITORPAGE_H
#include "../model/channelconfig.h"
#include "../service/authsession.h"
#include <QFrame>
#include <QImage>
#include <opencv2/core.hpp>
class QLabel;
class QPushButton;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QGridLayout;
// 只拥有显示状态和截图用的最新源帧，不拥有后台线程。
class MonitorPage : public QFrame {
    Q_OBJECT
public:
    explicit MonitorPage(QWidget* parent = nullptr);
    void setChannels(const QList<ChannelConfig>& channels);
    void setAccessEnabled(bool enabled);
    void setImageDirectory(const QString& path)
    {
        image_directory_ = path;
    }
    void setDetectionState(bool enabled, bool allowed);
    void setGpuPreference(bool enabled);
public slots:
    void setResourceUsage(double cpu, double gpu, double memory);
    void display_frame(const cv::Mat& frame, int channel);
    void on_capture_state_changed(int channel, bool running);
    void show_capture_error(int channel, const QString& message);
    void show_single_window();
    void show_multi_window();
    void showMessage(const QString& message);
    void setChannelMessage(int channel, const QString& message);
signals:
    void refreshCamerasRequested();
    void detectionToggleRequested(bool enabled);
    void gpuToggleRequested(bool enabled);
    void previewFramePresented(int channel);
    void auditRequested(const QString& action, const QString& detail);

private:
    void initUi();
    void select_channel(QListWidgetItem* item);
    void refresh_channel_list();
    void take_preview_snapshot();
    bool require_permission(AuthPermission, const QString&);
    static QImage frameImage(const cv::Mat& frame);
    bool accessEnabled_ = false, detectionAllowed_ = true, single_mode_ = false;
    bool preview_running_[4] { };
    int selected_channel_ = 1;
    QList<ChannelConfig> channel_configs_;
    cv::Mat latest_source_frames_[4];
    QString image_directory_;
    QLabel* video_windows[4] { };
    QGridLayout* video_layout { };
    QPushButton* btn_one { };
    QPushButton* btn_four { };
    QPushButton* btn_refresh_cameras { };
    QListWidget* device_list { };
    QLabel* lab_video_hint { };
    QLabel* lab_storage_warning { };
    QLabel* resource_cpu_ { };
    QLabel* resource_gpu_ { };
    QLabel* resource_memory_ { };
    QCheckBox* detection_enabled_box_ { };
    QCheckBox* gpu_enabled_box_ { };
};
#endif
