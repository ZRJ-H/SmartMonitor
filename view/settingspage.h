#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H
#include "../service/authsession.h"
#include "../service/channelconfigservice.h"
#include "../service/systemconfigservice.h"
#include <QFrame>
#include <QStringList>
class QLineEdit;
class QComboBox;
class QPushButton;
class QMediaDevices;
class QDoubleSpinBox;
// 控件与已保存配置归设置页管理；成功保存后发布值，不接触采集线程。
class SettingsPage : public QFrame {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);
    void setAccessEnabled(bool enabled)
    {
        accessEnabled_ = enabled;
        setEnabled(enabled);
    }
    QList<ChannelConfig> channels() const
    {
        return channel_configs_;
    }
    QString startupMessage() const
    {
        return startupWarnings_.join("\n");
    }
    SystemConfig storage() const
    {
        return storage_;
    }
    bool detectionEnabled() const
    {
        return detection_enabled_value_;
    }
    QString modelPath() const
    {
        return yolo_model_path_;
    }
    double confidence() const
    {
        return detection_confidence_value_;
    }
public slots:
    void refresh_camera_devices();
    void setDetectionEnabled(bool enabled);
    void setBackupRunning(bool running);
signals:
    void backupRequested();
    void channelsSaved(const QList<ChannelConfig>& channels);
    void storageSaved(const SystemConfig& config);
    void detectionSaved(bool enabled, const QString& model, double confidence);
    void warning(const QString& message);
    void detectionToggleRejected(bool currentState);
    void auditRequested(const QString& action, const QString& detail, int actorId = -1);

private:
    bool require_permission(AuthPermission, const QString& action);
    void initUi();
    void load_system_settings();
    void load_channel_configs();
    void apply_channel_configs_to_ui();
    QList<ChannelConfig> collect_channel_configs_from_ui() const;
    void update_source_controls(int index);
    void choose_channel_file(int index);
    void browse_record_directory();
    void save_recording_settings();
    void browse_yolo_model();
    void save_detection_settings();
    void save_channel_configs();
    QPushButton* backupButton_ = nullptr;
    bool accessEnabled_ = false;
    SystemConfig storage_;
    QStringList startupWarnings_;
    QList<ChannelConfig> channel_configs_;
    ChannelConfigService channel_config_service_;
    ConfigModel config_model_;
    QString record_directory_, yolo_model_path_;
    int record_segment_seconds_value_ = 10;
    bool detection_enabled_value_ = false;
    double detection_confidence_value_ = 0.5;
    QLineEdit* channel_name_edits_[4] { };
    QComboBox* channel_source_boxes_[4] { };
    QComboBox* channel_camera_boxes_[4] { };
    QLineEdit* channel_file_edits_[4] { };
    QPushButton* channel_file_buttons_[4] { };
    QMediaDevices* media_devices_ { };
    QLineEdit* record_directory_edit_ { };
    QComboBox* record_segment_seconds_ { };
    QLineEdit* yolo_model_edit_ { };
    QDoubleSpinBox* detection_confidence_spin_ { };
    QLineEdit* image_directory_edit_ { };
    QPushButton* btn_save_channel_configs { };
    QPushButton* btn_save_recording_settings { };
    QPushButton* btn_save_detection_settings { };
};
#endif
