// 【阅读导引】
// 回放页面：录像查询、播放控制、本地视频事件和关联跳转。
// 本页拥有自己的控件和状态，通过公开接口及信号与其他模块交互。

#include "playbackpage.h"
#include "../service/imageservice.h"
#include "../util/dbconn.h"
#include <QAudioOutput>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>

PlaybackPage::PlaybackPage(QWidget* parent)
    : QFrame(parent)
    , video_service(5)
{
    playback_player_ = new QMediaPlayer(this);
    playback_audio_ = new QAudioOutput(this);
    initUi();
    connect(btn_refresh_records, SIGNAL(clicked()), this, SLOT(load_video_records()));
    connect(btn_previous_video_page, SIGNAL(clicked()), this, SLOT(load_previous_video_page()));
    connect(btn_next_video_page, SIGNAL(clicked()), this, SLOT(load_next_video_page()));
    connect(video_record_list, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem* item, QListWidgetItem*) {
            if (item && item->data(Qt::UserRole + 2).toBool())
                play_selected_record();
        });
    connect(video_record_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item->data(Qt::UserRole + 2).toBool())
            play_selected_record();
    });
    connect(btn_play_record, &QPushButton::clicked, this, &PlaybackPage::togglePlayStop);
    connect(btn_pause_playback, &QPushButton::clicked, this, &PlaybackPage::togglePauseResume);
    connect(playback_player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState) playback_requested_ = false;
        updatePlaybackControls();
    });
    connect(playback_player_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &error) {
        playback_requested_ = false;
        pending_playback_offset_ = -1;
        lab_playback_status->setText("回放失败：" + error);
        updatePlaybackControls();
    });
    connect(btn_playback_snapshot, &QPushButton::clicked, this, &PlaybackPage::take_playback_snapshot);
    connect(playback_slider_, &QSlider::sliderReleased, this,
        [this]() { seek_playback(playback_slider_->value()); });
    connect(playback_speed_, qOverload<int>(&QComboBox::currentIndexChanged), this,
        &PlaybackPage::change_playback_speed);
    connect(playback_player_, &QMediaPlayer::positionChanged, this, &PlaybackPage::update_playback_position);
    connect(playback_player_, &QMediaPlayer::durationChanged, this, &PlaybackPage::update_playback_duration);
    connect(
        playback_player_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia) {
                playback_requested_ = false;
                pending_playback_offset_ = -1;
            }
            updatePlaybackControls();
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
                if (pending_playback_offset_ >= 0) {
                    playback_player_->setPosition(
                        qBound<qint64>(0, pending_playback_offset_, playback_player_->duration()));
                    pending_playback_offset_ = -1;
                }
            }
        });
    setAccessEnabled(false);
}
void PlaybackPage::setAccessEnabled(bool enabled)
{
    accessEnabled_ = enabled;
    setEnabled(enabled);
    if (!enabled)
        resetSession();
    updatePlaybackControls();
}
void PlaybackPage::resetSession()
{
    ++source_playback_request_;
    playback_requested_ = false;
    pending_playback_offset_ = -1;
    playback_player_->stop();
    updatePlaybackControls();
    playback_player_->setSource(QUrl());
    current_playback_ = VideoRecord();
    current_video_page = 1;
    video_record_list->clear();
    source_event_list_->clear();
    lab_video_page->setText("第 1 / 1 页，共 0 条");
    for (auto dialog : findChildren<QDialog*>())
        dialog->close();
}
bool PlaybackPage::require_permission(AuthPermission, const QString& action)
{
    if (accessEnabled_)
        return true;
    QMessageBox::information(this, "需要登录", "请先登录，再" + action + "。");
    return false;
}
// 创建录像回放页面；控件名称与对象归属保持原约定。
void PlaybackPage::initUi()
{
    // ==================== 第 1 页：回放页面 ====================
    QWidget* playback_page = this;
    playback_page->setObjectName("playbackPage");
    QVBoxLayout* playback_layout = new QVBoxLayout(playback_page);
    playback_layout->setContentsMargins(28, 22, 28, 28);
    playback_layout->setSpacing(18);

    QLabel* playback_title = new QLabel("录像回放");
    playback_title->setObjectName("pageTitle");
    playback_layout->addWidget(playback_title);

    QHBoxLayout* playback_content = new QHBoxLayout;
    playback_content->setSpacing(20);

    // 左侧列表只显示数据库 video 表中的文件名。
    QFrame* record_panel = new QFrame;
    record_panel->setObjectName("recordFilterPanel");
    record_panel->setMinimumWidth(250);
    record_panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto record_outer = new QVBoxLayout(record_panel);
    source_tabs_ = new QTabWidget;
    source_tabs_->setObjectName("playbackSourceTabs");
    record_outer->addWidget(source_tabs_);
    auto recordedPage = new QWidget;
    auto indexedPage = new QWidget;
    recordedPage->setObjectName("recordedSourcePage");
    indexedPage->setObjectName("indexedSourcePage");
    source_tabs_->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #465365; background: #202830; } "
        "QTabBar::tab { background: #283443; color: #dce5ef; padding: 9px 12px; border: 1px solid #465365; } "
        "QTabBar::tab:selected { background: #315b89; color: white; } "
        "QWidget#recordedSourcePage, QWidget#indexedSourcePage { background: #202830; } "
        "QLabel, QCheckBox { color: #dce5ef; }");
    source_tabs_->addTab(recordedPage, "摄像头 / 历史录像");
    source_tabs_->addTab(indexedPage, "本地视频事件");
    QVBoxLayout* record_layout = new QVBoxLayout(recordedPage);
    auto source_layout = new QVBoxLayout(indexedPage);

    QHBoxLayout* record_header_layout = new QHBoxLayout;
    QLabel* record_title = new QLabel("视频文件列表");
    record_title->setObjectName("sectionTitle");
    btn_refresh_records = new QPushButton("刷新记录");
    btn_refresh_records->setObjectName("videoRecordRefreshButton");
    btn_refresh_records->setMinimumHeight(34);
    record_header_layout->addWidget(record_title);
    record_header_layout->addStretch();
    record_header_layout->addWidget(btn_refresh_records);
    record_layout->addLayout(record_header_layout);

    QGridLayout* filter_layout = new QGridLayout;
    filter_layout->setObjectName("playbackFilterGrid");
    query_start_time_ = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1));
    query_end_time_ = new QDateTimeEdit(QDateTime::currentDateTime());
    query_start_time_->setCalendarPopup(true);
    query_end_time_->setCalendarPopup(true);
    query_start_time_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    query_end_time_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    query_channel_ = new QComboBox;
    query_channel_->addItem("全部通道", 0);
    for (int channel = 1; channel <= 4; ++channel)
        query_channel_->addItem(QStringLiteral("通道 %1").arg(channel), channel);
    query_type_ = new QComboBox;
    query_type_->addItem("全部录像", -1);
    query_type_->addItem("普通录像", 0);
    query_type_->addItem("事件录像", 1);
    query_start_time_->setObjectName("playbackStartTime");
    query_end_time_->setObjectName("playbackEndTime");
    query_start_time_->setToolTip("开始时间（年-月-日 时:分:秒）");
    query_end_time_->setToolTip("结束时间（年-月-日 时:分:秒）");
    // 时间与类别分成两行，忽略默认宽度提示，防止日期编辑器撑宽左栏。
    for (auto date : { query_start_time_, query_end_time_ }) {
        date->setDisplayFormat("yy-MM-dd HH:mm:ss");
        date->setMinimumWidth(0);
        date->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
    for (auto combo : { query_channel_, query_type_ }) {
        combo->setMinimumWidth(0);
        combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
    filter_layout->addWidget(query_start_time_, 0, 0);
    filter_layout->addWidget(query_channel_, 0, 1);
    filter_layout->addWidget(query_end_time_, 1, 0);
    filter_layout->addWidget(query_type_, 1, 1);
    filter_layout->setColumnStretch(0, 2);
    filter_layout->setColumnStretch(1, 1);
    record_layout->addLayout(filter_layout);

    video_record_list = new QListWidget;
    video_record_list->setObjectName("recordList");
    video_record_list->setAlternatingRowColors(true);
    record_layout->addWidget(video_record_list, 1);

    QHBoxLayout* page_layout = new QHBoxLayout;
    btn_previous_video_page = new QPushButton("上一页");
    btn_next_video_page = new QPushButton("下一页");
    lab_video_page = new QLabel("第 1 / 1 页，共 0 条");
    btn_previous_video_page->setObjectName("controlButton");
    btn_next_video_page->setObjectName("controlButton");
    lab_video_page->setObjectName("settingText");
    page_layout->addWidget(btn_previous_video_page);
    page_layout->addStretch();
    page_layout->addWidget(lab_video_page);
    page_layout->addStretch();
    page_layout->addWidget(btn_next_video_page);
    record_layout->addLayout(page_layout);
    auto sourceTitle = new QLabel("单击回放原视频，双击查看事件截图");
    sourceTitle->setWordWrap(true);
    source_layout->addWidget(sourceTitle);
    source_channel_ = new QComboBox;
    source_channel_->addItem("全部通道", 0);
    for (int channel = 1; channel <= 4; ++channel)
        source_channel_->addItem(QString("通道 %1").arg(channel), channel);
    source_layout->addWidget(source_channel_);
    connect(source_channel_, &QComboBox::currentIndexChanged, this, [this] {
        source_page_ = 0;
        refresh_source_events();
    });
    auto sourceRefresh = new QPushButton("刷新事件列表");
    sourceRefresh->setObjectName("sourceEventRefreshButton");
    source_layout->addWidget(sourceRefresh);
    connect(sourceRefresh, &QPushButton::clicked, this, &PlaybackPage::refresh_source_events);
    source_history_ = new QCheckBox("显示以前的视频和检测结果");
    source_layout->addWidget(source_history_);
    source_history_->setToolTip("包含此通道以前使用的视频，以及更换检测设置前的结果");
    connect(source_history_, &QCheckBox::toggled, this, [this] {
        source_page_ = 0;
        refresh_source_events();
    });
    source_event_list_ = new QListWidget;
    source_event_list_->setObjectName("sourceEventList");
    source_event_list_->setWordWrap(true);
    source_event_list_->setTextElideMode(Qt::ElideMiddle);
    source_event_list_->setMinimumHeight(100);
    source_layout->addWidget(source_event_list_, 1);
    auto sourcePages = new QHBoxLayout;
    auto previous = new QPushButton("上一页");
    auto next = new QPushButton("下一页");
    source_page_label_ = new QLabel("0 条");
    sourcePages->addWidget(previous);
    sourcePages->addWidget(source_page_label_, 1);
    sourcePages->addWidget(next);
    source_layout->addLayout(sourcePages);
    auto relocate = new QPushButton("重新选择原视频");
    relocate->setObjectName("relocateSourceButton");
    source_layout->addWidget(relocate);
    relocate->setToolTip("视频移动或改名后，重新选择同一视频文件");
    connect(previous, &QPushButton::clicked, this, [this] {
        source_page_ = qMax(0, source_page_ - 1);
        refresh_source_events();
    });
    connect(next, &QPushButton::clicked, this, [this] {
        ++source_page_;
        refresh_source_events();
    });
    connect(source_event_list_, &QListWidget::itemClicked, this, &PlaybackPage::play_source_event);
    connect(source_event_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!accessEnabled_)
            return;
        const auto row = item->data(Qt::UserRole).value<SourceEventRow>();
        auto dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(
            "事件截图 · 视频时间 " + QString::number(row.startMs / 1000.0, 'f', 3) + " 秒");
        auto layout = new QVBoxLayout(dialog);
        auto image = new QLabel;
        const QPixmap pixmap(row.image);
        if (pixmap.isNull())
            image->setText("截图文件不可用");
        else
            image->setPixmap(pixmap.scaled(1000, 700, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(image);
        dialog->show();
    });
    connect(relocate, &QPushButton::clicked, this, [this] {
        if (!require_permission(AuthPermission::Settings, "重新定位原视频")
            || !source_event_list_->currentItem())
            return;
        const auto row = source_event_list_->currentItem()->data(Qt::UserRole).value<SourceEventRow>();
        const QString path = QFileDialog::getOpenFileName(this, "选择同一内容的原视频");
        if (path.isEmpty())
            return;
        emit sourceRelocationRequested(row.asset, path);
    });
    playback_content->addWidget(record_panel, 3);

    QFrame* playback_panel = new QFrame;
    playback_panel->setObjectName("playbackDisplayPanel");
    QVBoxLayout* playback_view_layout = new QVBoxLayout(playback_panel);
    playback_video_ = new QVideoWidget;
    playback_video_->setObjectName("playbackWindow");
    playback_video_->setMinimumSize(360, 260);
    playback_player_->setAudioOutput(playback_audio_);
    playback_player_->setVideoOutput(playback_video_);
    playback_audio_->setVolume(0.7);
    playback_view_layout->addWidget(playback_video_, 1);

    lab_playback_status = new QLabel("请选择左侧录像文件进行回放");
    lab_playback_status->setObjectName("settingText");
    playback_view_layout->addWidget(lab_playback_status);
    playback_slider_ = new QSlider(Qt::Horizontal);
    playback_slider_->setRange(0, 1000);
    playback_view_layout->addWidget(playback_slider_);

    QHBoxLayout* playback_controls = new QHBoxLayout;
    btn_play_record = new QPushButton("播放");
    btn_pause_playback = new QPushButton("暂停");
    btn_play_record->setObjectName("playStopButton");
    btn_pause_playback->setObjectName("pauseResumeButton");
    btn_playback_snapshot = new QPushButton("回放截图");
    playback_speed_ = new QComboBox;
    playback_speed_->addItem("1×", 1.0);
    playback_speed_->addItem("2×", 2.0);
    playback_speed_->addItem("4×", 4.0);
    const QList<QPushButton*> playback_buttons = { btn_play_record, btn_pause_playback, btn_playback_snapshot };
    for (QPushButton* button : playback_buttons) {
        if (button->objectName().isEmpty()) button->setObjectName("controlButton");
        button->setMinimumHeight(38);
        playback_controls->addWidget(button);
    }
    playback_controls->addWidget(playback_speed_);
    playback_view_layout->addLayout(playback_controls);
    playback_content->addWidget(playback_panel, 7);
    playback_layout->addLayout(playback_content, 1);
}

// 按当前筛选重新加载录像列表，交由分页服务校验条件。
void PlaybackPage::load_video_records()
{
    refresh_source_events();
    current_video_page = 1;
    load_video_page(current_video_page);
}

// 把控件值整理成 VideoQuery；查询条件对象本身不执行 SQL。
VideoQuery PlaybackPage::current_video_query() const
{
    VideoQuery query;
    query.startTime = query_start_time_->dateTime();
    query.endTime = query_end_time_->dateTime();
    query.channelId = query_channel_->currentData().toInt();
    query.eventType = query_type_->currentData().toInt();
    return query;
}

// 加载成功才更新列表和页码；每个条目携带录像 ID，回放不靠猜文件名。
bool PlaybackPage::load_video_page(int requestedPage)
{
    if (!require_permission(AuthPermission::Playback, "查询录像记录")) {
        return false;
    }

    VideoPage page;
    QString error_text;
    if (!video_service.loadPage(current_video_query(), requestedPage, page, error_text)) {
        QMessageBox::warning(this, "视频查询失败", error_text);
        return false;
    }

    // 只有查询成功后才提交页码，避免失败时状态与画面不一致。
    current_video_page = page.currentPage;
    video_record_list->clear();
    for (const VideoRecord& record : page.records) {
        QListWidgetItem* item = new QListWidgetItem(QStringLiteral("%1 | 通道%2 | %3%4")
                .arg(record.startTime.toString("yyyy-MM-dd HH:mm:ss"))
                .arg(record.channelId)
                .arg(record.name)
                .arg(record.eventVideo ? QStringLiteral(" [事件]") : QString()));
        item->setData(Qt::UserRole, record.path);
        item->setData(Qt::UserRole + 1, record.id);
        item->setData(Qt::UserRole + 2, record.eventVideo);
        item->setToolTip(QString("通道 %1\n%2\n开始：%3\n结束：%4\n大小：%5 字节\n%6")
                .arg(record.channelId)
                .arg(record.name.toHtmlEscaped())
                .arg(record.startTime.toString("yyyy-MM-dd HH:mm:ss"))
                .arg(record.endTime.toString("yyyy-MM-dd HH:mm:ss"))
                .arg(record.fileSize)
                .arg(record.eventVideo ? "事件录像：选中即可回放" : "普通录像：双击回放"));
        video_record_list->addItem(item);
    }
    lab_video_page->setText(
        QString("%1 / %2 页 · %3 条").arg(page.currentPage).arg(page.totalPages).arg(page.totalRecords));
    btn_previous_video_page->setEnabled(page.currentPage > 1);
    btn_next_video_page->setEnabled(page.currentPage < page.totalPages);
    return true;
}

// 请求上一页；有效页码的边界由 load_video_page/VideoService 处理。
void PlaybackPage::load_previous_video_page()
{
    load_video_page(current_video_page - 1);
}

// 请求下一页，成功后才更新界面显示的当前页。
void PlaybackPage::load_next_video_page()
{
    load_video_page(current_video_page + 1);
}

// 验证权限与选中项，然后按数据库 ID 打开关联录像。
void PlaybackPage::play_selected_record()
{
    ++source_playback_request_;
    if (!require_permission(AuthPermission::Playback, "播放录像"))
        return;
    if (source_tabs_->currentIndex() == 1) {
        auto item = source_event_list_->currentItem();
        if (!item) {
            lab_playback_status->setText("请先选择左侧事件");
            return;
        }
        play_source_event(item);
        return;
    }
    auto item = video_record_list->currentItem();
    if (!item) {
        lab_playback_status->setText("请先选择录像");
        return;
    }
    open_linked_video(item->data(Qt::UserRole + 1).toLongLong(), 0);
}

void PlaybackPage::updatePlaybackControls()
{
    const auto state = playback_player_->playbackState();
    const bool failed = playback_player_->error() != QMediaPlayer::NoError
        || playback_player_->mediaStatus() == QMediaPlayer::InvalidMedia;
    const bool ended = playback_player_->mediaStatus() == QMediaPlayer::EndOfMedia;
    const bool active = !failed && !ended && (playback_requested_ || state != QMediaPlayer::StoppedState);
    btn_play_record->setText(active ? "停止" : "播放");
    btn_play_record->setToolTip(active ? "停止当前回放或取消待处理的播放请求" : "播放选中的录像");
    btn_play_record->setEnabled(accessEnabled_);
    btn_pause_playback->setText(state == QMediaPlayer::PausedState ? "继续" : "暂停");
    btn_pause_playback->setEnabled(accessEnabled_ && active && state != QMediaPlayer::StoppedState);
}
void PlaybackPage::togglePlayStop()
{
    if (!accessEnabled_) return;
    const auto state = playback_player_->playbackState();
    if (playback_requested_ || state != QMediaPlayer::StoppedState) {
        stop_playback();
        return;
    }
    // Linked playback may have no selected row in the current list.
    const bool selected = source_tabs_->currentIndex() == 1
        ? source_event_list_->currentItem() != nullptr : video_record_list->currentItem() != nullptr;
    if (!selected && !current_playback_.path.isEmpty() && !playback_player_->source().isEmpty()) {
        playback_requested_ = true;
        playback_player_->setPosition(0);
        playback_player_->play();
        updatePlaybackControls();
    } else play_selected_record();
}
void PlaybackPage::togglePauseResume()
{
    if (!accessEnabled_) return;
    if (playback_player_->playbackState() == QMediaPlayer::PausedState) resume_playback();
    else if (playback_player_->playbackState() == QMediaPlayer::PlayingState) pause_playback();
}

// 登录后暂停播放器，保留当前源与位置以便继续。
void PlaybackPage::pause_playback()
{
    if (accessEnabled_)
        playback_player_->pause();
}

// 仅在登录且有有效录像记录时继续播放，避免退出后恢复旧源。
void PlaybackPage::resume_playback()
{
    if (accessEnabled_ && !current_playback_.path.isEmpty())
        playback_player_->play();
}

// 停止当前播放器；这与停止实时采集或录像线程无关。
void PlaybackPage::stop_playback()
{
    ++source_playback_request_;
    playback_requested_ = false;
    pending_playback_offset_ = -1;
    playback_player_->stop();
    updatePlaybackControls();
}

// 读取组合框中的倍率并设置播放器播放速率，不改变文件编码 FPS。
void PlaybackPage::change_playback_speed(int index)
{
    Q_UNUSED(index);
    playback_player_->setPlaybackRate(playback_speed_->currentData().toDouble());
}

// 把毫秒位置换算为滑块比例；用户正在拖动时不抢占滑块。
void PlaybackPage::update_playback_position(qint64 position)
{
    if (!playback_slider_->isSliderDown() && playback_duration_ > 0)
        playback_slider_->setValue(static_cast<int>(position * 1000 / playback_duration_));
}

// 缓存播放器异步报告的时长，供进度显示与拖动定位使用。
void PlaybackPage::update_playback_duration(qint64 duration)
{
    playback_duration_ = duration;
}

// 把滑块比例换算成毫秒后请求定位；有有效时长才计算。
void PlaybackPage::seek_playback(int value)
{
    if (playback_duration_ > 0)
        playback_player_->setPosition(playback_duration_ * value / 1000);
}

// 从视频输出当前帧取图，保存真实录像 ID 与当前播放位置。
void PlaybackPage::take_playback_snapshot()
{
    if (!require_permission(AuthPermission::Playback, "保存回放截图"))
        return;
    const QImage frame = playback_video_->videoSink()->videoFrame().toImage();
    FeatureImage saved;
    QString error;
    ImageService service(DbConn::getInstance()->database(), image_directory_);
    if (current_playback_.id <= 0
        || !service.saveManualImage(frame, current_playback_.channelId, QDateTime::currentDateTime(),
            current_playback_.id, playback_player_->position(), saved, error)) {
        lab_playback_status->setText(error.isEmpty() ? "当前没有可保存的回放帧" : error);
        return;
    }
    emit auditRequested("截图", saved.imagePath);
    lab_playback_status->setText("截图已保存到图片管理");
}

// 按录像 ID 查路径并确认文件存在；播放器就绪后定位，同源跳转单独处理。
void PlaybackPage::open_linked_video(qint64 id, qint64 offset)
{
    ++source_playback_request_;
    if (!require_permission(AuthPermission::Playback, "打开关联录像"))
        return;
    VideoRecord record;
    QString error;
    if (!video_service.findById(id, record, error) || !QFileInfo::exists(record.path)) {
        playback_requested_ = false;
        updatePlaybackControls();
        playback_player_->stop();
        playback_player_->setSource(QUrl());
        current_playback_ = VideoRecord();
        pending_playback_offset_ = -1;
        // 回放失败必须出现在回放页，不能只更新隐藏的监控页提示。
        emit showRequested();
        lab_playback_status->setText(error.isEmpty() ? "录像文件不存在" : error);
        return;
    }
    btn_playback_snapshot->setEnabled(true);
    btn_playback_snapshot->setToolTip(QString());
    playback_requested_ = false;
    current_playback_ = record;
    pending_playback_offset_ = qMax<qint64>(0, offset);
    lab_playback_status->setText(QStringLiteral("当前录像：%1").arg(record.name));
    emit showRequested();
    playback_player_->stop();
    const QUrl source = QUrl::fromLocalFile(record.path);
    if (playback_player_->source() == source) {
        playback_player_->setPosition(pending_playback_offset_);
        pending_playback_offset_ = -1;
    } else {
        playback_player_->setSource(source);
    }
    playback_player_->setPlaybackRate(playback_speed_->currentData().toDouble());
    playback_requested_ = true;
    playback_player_->play();
    updatePlaybackControls();
    emit auditRequested("回放", record.path);
}

void PlaybackPage::refresh_source_events()
{
    if (!accessEnabled_)
        return;
    emit sourceEventsRequested(
        source_page_, source_channel_->currentData().toInt(), source_history_->isChecked());
}

void PlaybackPage::play_source_event(QListWidgetItem* item)
{
    if (!item || !require_permission(AuthPermission::Playback, "回放文件事件"))
        return;
    const quint64 request = ++source_playback_request_;
    const auto row = item->data(Qt::UserRole).value<SourceEventRow>();
    btn_playback_snapshot->setEnabled(false);
    btn_playback_snapshot->setToolTip("双击左侧事件即可查看已保存的截图");
    playback_requested_ = false;
    source_tabs_->setCurrentIndex(1);
    playback_player_->stop();
    playback_player_->setSource(QUrl());
    current_playback_ = VideoRecord();
    pending_playback_offset_ = -1;
    updatePlaybackControls();
    if (!QFileInfo::exists(row.path)) {
        lab_playback_status->setText("找不到原视频，请点击“重新选择原视频”");
        return;
    }
    playback_requested_ = true;
    updatePlaybackControls();
    lab_playback_status->setText("正在校验原视频…");
    emit sourcePlaybackRequested(row, request);
}
void PlaybackPage::receiveSourceEvents(const QList<SourceEventRow>& rows, int page, int total)
{
    if (!accessEnabled_)
        return;
    const QSignalBlocker blocker(source_event_list_);
    source_event_list_->clear();
    source_page_ = page;
    source_page_label_->setText(
        QString("%1/%2 · %3 条").arg(page + 1).arg(qMax(1, (total + 49) / 50)).arg(total));
    for (const auto& r : rows) {
        auto item = new QListWidgetItem(QString("%1\n关联通道 %2 · 事件时间 %3–%4 秒")
                                            .arg(QFileInfo(r.path).fileName(), r.channels)
                                            .arg(r.startMs / 1000.0, 0, 'f', 1)
                                            .arg(r.endMs / 1000.0, 0, 'f', 1),
            source_event_list_);
        item->setData(Qt::UserRole, QVariant::fromValue(r));
        item->setToolTip(
            QString("原视频：%1\n分析时间：%2\n单击回放，双击查看截图").arg(r.path, r.analyzedAt));
    }
}
void PlaybackPage::receiveSourcePlayback(const SourceEventRow& row, quint64 request, const QString& error)
{
    if (request != source_playback_request_ || !accessEnabled_)
        return;
    if (!error.isEmpty()) {
        playback_requested_ = false;
        updatePlaybackControls();
        lab_playback_status->setText(error);
        return;
    }
    current_playback_.path = row.path;
    current_playback_.channelId = row.channels.section(',', 0, 0).toInt();
    pending_playback_offset_ = qMax<qint64>(0, row.startMs - 2000);
    playback_player_->setSource(QUrl::fromLocalFile(row.path));
    playback_requested_ = true;
    playback_player_->play();
    updatePlaybackControls();
    lab_playback_status->setText(
        QString("正在回放原视频 · 事件位于 %1 秒").arg(row.startMs / 1000.0, 0, 'f', 3));
}
