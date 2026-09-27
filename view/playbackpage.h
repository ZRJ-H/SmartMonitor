#ifndef PLAYBACKPAGE_H
#define PLAYBACKPAGE_H
#include "../service/authsession.h"
#include "../service/sourceeventrow.h"
#include "../service/videoservice.h"
#include <QFrame>
class QPushButton;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QDateTimeEdit;
class QComboBox;
class QVideoWidget;
class QMediaPlayer;
class QAudioOutput;
class QSlider;
class QTabWidget;
class QCheckBox;
// 独立拥有播放器、查询条件和回放状态；通过信号请求后台文件分析。
class PlaybackPage : public QFrame {
    Q_OBJECT
public:
    explicit PlaybackPage(QWidget* parent = nullptr);
    void setAccessEnabled(bool enabled);
    void resetSession();
    void setImageDirectory(const QString& path)
    {
        image_directory_ = path;
    }
public slots:
    void load_video_records();
    void open_linked_video(qint64 id, qint64 offset);
    void receiveSourceEvents(const QList<SourceEventRow>& rows, int page, int total);
    void receiveSourcePlayback(const SourceEventRow& row, quint64 request, const QString& error);
    void refresh_source_events();
    void play_selected_record();
    void pause_playback();
    void resume_playback();
    void stop_playback();
signals:
    void showRequested();
    void auditRequested(const QString& action, const QString& detail);
    void sourceEventsRequested(int page, int channel, bool history);
    void sourcePlaybackRequested(const SourceEventRow& row, quint64 request);
    void sourceRelocationRequested(const QString& asset, const QString& path);
private slots:
    void load_previous_video_page();
    void load_next_video_page();
    void change_playback_speed(int index);
    void update_playback_position(qint64 position);
    void update_playback_duration(qint64 duration);
    void seek_playback(int value);
    void take_playback_snapshot();
    void play_source_event(QListWidgetItem* item);

private:
    void initUi();
    bool require_permission(AuthPermission, const QString& action);
    bool load_video_page(int page);
    VideoQuery current_video_query() const;
    bool accessEnabled_ = false;
    QString image_directory_;
    VideoService video_service;
    int current_video_page = 1, source_page_ = 0;
    quint64 source_playback_request_ = 0;
    qint64 playback_duration_ = 0, pending_playback_offset_ = -1;
    VideoRecord current_playback_;
    QPushButton* btn_refresh_records;
    QPushButton* btn_play_record;
    QPushButton* btn_pause_playback;
    QPushButton* btn_resume_playback;
    QPushButton* btn_stop_playback;
    QPushButton* btn_playback_snapshot;
    QLabel* lab_playback_status;
    QListWidget* video_record_list;
    QLabel* lab_video_page;
    QPushButton* btn_previous_video_page;
    QPushButton* btn_next_video_page;
    QDateTimeEdit* query_start_time_;
    QDateTimeEdit* query_end_time_;
    QComboBox* query_channel_;
    QVideoWidget* playback_video_;
    QMediaPlayer* playback_player_;
    QAudioOutput* playback_audio_;
    QSlider* playback_slider_;
    QComboBox* playback_speed_;
    QComboBox* query_type_ = nullptr;
    QListWidget* source_event_list_ = nullptr;
    QTabWidget* source_tabs_ = nullptr;
    QComboBox* source_channel_ = nullptr;
    QCheckBox* source_history_ = nullptr;
    QLabel* source_page_label_ = nullptr;
};
#endif
