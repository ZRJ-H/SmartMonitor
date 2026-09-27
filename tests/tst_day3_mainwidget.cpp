// 主窗口界面测试：验证启动后的四画面布局及通道控制、四行配置控件是否齐全。
#include <QtTest>
#include <QThread>
#include <QTimer>
#include <QMessageBox>
#include "../manager/windowmanager.h"
#include "../view/loginwidget.h"
#include <QTemporaryDir>
#include <QFontDatabase>
#include <QScrollArea>
#include <QAbstractSpinBox>
#include <QScrollBar>
#include <QStandardPaths>
#include <QScopeGuard>
#include "../service/channelconfigpolicy.h"
#include <QMediaPlayer>
#include <opencv2/videoio.hpp>
#include "testdatabase.h"

#include "../view/mainwidget.h"
#include "../view/playbackpage.h"
#include "../view/monitorpage.h"
#include "../view/settingspage.h"
#include "../service/monitoringcontroller.h"
#include "../service/maintenancecontroller.h"
#include "../service/channelconfigservice.h"
#include "../model/videomodel.h"
#include "../service/sourceanalysisworker.h"
#include "../model/configmodel.h"

#include <QLabel>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QStackedWidget>
#include <QVideoSink>
#include "../view/operationlogpage.h"
#include "../view/imagepage.h"

namespace {
// 所有后台测试只使用内存数据库和临时 AVI，不打开摄像头或生产文件。
bool prepareBackgroundSources(const QTemporaryDir &directory, QString &error)
{
    const QString source = directory.filePath("source.avi");
    cv::VideoWriter writer(source.toStdString(), cv::VideoWriter::fourcc('M','J','P','G'),
                           12, cv::Size(320,240));
    if (!writer.isOpened()) { error = "Cannot create synthetic AVI"; return false; }
    // 红色随真实源帧单调递增，可区分持续解码、重复末帧和重新打开文件。
    for (int frame=0; frame<96; ++frame)
        writer.write(cv::Mat(240,320,CV_8UC3,cv::Scalar(20,40,30+frame*2)));
    writer.release();
    auto channels = ChannelConfigPolicy::defaults();
    for (auto &channel : channels) {
        channel.sourceType = ChannelSourceType::File;
        channel.filePath = source;
    }
    ChannelConfigService service;
    if (!service.save(channels,error)) return false;
    ConfigModel configuration;
    return configuration.upsertValue("record.directory",directory.filePath("recordings"))
            && configuration.upsertValue("record.segment_seconds","60")
            && configuration.upsertValue("image_dir",directory.filePath("images"))
            && configuration.upsertValue("detection.enabled","0");
}

void clearBackgroundTestData()
{
    QSqlQuery query(DbConn::getInstance()->database());
    for (const char *table : {"event_media_link", "feature_image", "monitor_event",
                              "exception_log", "video", "sys_config"})
        query.exec(QString("DELETE FROM %1").arg(table));
}

bool allPreviewFramesAreHidden(const MainWidget &widget)
{
    for (auto window : widget.findChildren<QLabel*>("videoWindow"))
        if (!window->pixmap().isNull()) return false;
    return true;
}
}

class Day3MainWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void logEventButtonsOpenMatchingImageAndRealVideo() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const QString path=dir.filePath("linked-event.avi");
        cv::VideoWriter writer(path.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),25,cv::Size(64,48));
        QVERIFY(writer.isOpened());for(int i=0;i<100;++i)writer.write(cv::Mat(48,64,CV_8UC3,cv::Scalar(20,70,120)));writer.release();
        QSqlQuery q(DbConn::getInstance()->database());
        const auto cleanup=qScopeGuard([&]{
            q.exec("DELETE FROM feature_image WHERE id IN (88001,88002)");
            q.exec("DELETE FROM event_media_link WHERE event_key='log-ui-event'");
            q.exec("DELETE FROM exception_log WHERE id=88001");q.exec("DELETE FROM video WHERE id=88001");
        });
        q.prepare("INSERT INTO video(id,video_name,video_path,channel_id,start_time,end_time,file_size,video_type) VALUES(88001,'linked-event',:path,1,:start,:end,100,'event')");
        q.bindValue(":path",path);q.bindValue(":start",QDateTime::currentDateTime().addSecs(-4));q.bindValue(":end",QDateTime::currentDateTime());QVERIFY(q.exec());
        q.prepare("INSERT INTO exception_log(id,channel_id,event_time,event_status,related_video_path) VALUES(88001,1,:time,'complete',:path)");
        q.bindValue(":time",QDateTime::currentDateTime());q.bindValue(":path",path);QVERIFY(q.exec());
        QVERIFY(q.exec("INSERT INTO event_media_link VALUES('log-ui-event',88001,88001)"));
        QImage frame(64,48,QImage::Format_RGB32);frame.fill(Qt::green);
        const auto imagePath=dir.filePath("event.jpg");QVERIFY(frame.save(imagePath));
        // 显式插入固定测试 ID，方便只清理本例创建的数据。
        q.prepare("INSERT INTO feature_image VALUES(88001,'event.jpg',:path,1,:time,'event',88001,88001,0)");
        q.bindValue(":path",imagePath);q.bindValue(":time",QDateTime::currentDateTime());QVERIFY(q.exec());
        QVERIFY(q.exec("INSERT INTO feature_image VALUES(88002,'unrelated.jpg','unrelated.jpg',2,'2026-09-24T00:00:00','event',88002,0,0)"));
        MainWidget widget;widget.resize(1280,760);widget.setLoggedInUser(17,"admin17");widget.show();
        auto logs=widget.findChild<OperationLogPage*>();auto stack=widget.findChild<QStackedWidget*>("pageStack");
        auto navigation=widget.findChild<QPushButton*>("logsNavigation");navigation->click();QVERIFY(navigation->isChecked());
        logs->findChild<QTabWidget*>()->setCurrentIndex(1);
        auto events=logs->findChild<QTableWidget*>("eventLogTable");
        int row=-1;for(int i=0;i<events->rowCount();++i)if(events->item(i,0)->text()=="88001")row=i;
        QVERIFY(row>=0);events->selectRow(row);QCOMPARE(stack->currentWidget(),static_cast<QWidget*>(logs));
        const auto capture=qEnvironmentVariable("SMART_MONITOR_UI_CAPTURE_DIR");
        if(!capture.isEmpty()){QDir().mkpath(capture);QTest::qWait(30);widget.grab().save(QDir(capture).filePath("log-events-reviewed.png"));}
        logs->findChild<QPushButton*>("viewEventImagesButton")->click();
        auto gallery=widget.findChild<ImagePage*>();QCOMPARE(stack->currentWidget(),static_cast<QWidget*>(gallery));
        QVERIFY(widget.findChild<QPushButton*>("imagesNavigation")->isChecked());
        auto thumbnails=gallery->findChild<QListWidget*>("imageThumbnails");QCOMPARE(thumbnails->count(),1);
        QVERIFY(thumbnails->item(0)->text().contains("event.jpg"));QVERIFY(!thumbnails->item(0)->icon().isNull());
        navigation->click();events->selectRow(row);
        auto player=widget.findChild<QMediaPlayer*>();QVERIFY(player->videoSink());
        QSignalSpy decoded(player->videoSink(),&QVideoSink::videoFrameChanged);
        logs->findChild<QPushButton*>("playEventVideoButton")->click();
        QCOMPARE(stack->currentWidget(),static_cast<QWidget*>(widget.findChild<PlaybackPage*>()));
        QTRY_COMPARE_WITH_TIMEOUT(player->source(),QUrl::fromLocalFile(path),2000);
        QTRY_COMPARE_WITH_TIMEOUT(player->playbackState(),QMediaPlayer::PlayingState,2000);
        QTRY_VERIFY_WITH_TIMEOUT(decoded.count()>0,3000);
        navigation->click();logs->findChild<QTabWidget*>()->setCurrentIndex(0);
        if(!capture.isEmpty()){QTest::qWait(30);widget.grab().save(QDir(capture).filePath("log-operations-reviewed.png"));}
    }

    void settingsAuditKeepsExplicitSystemActor() {
        MainWidget widget;widget.setLoggedInUser(17,"admin17");
        auto settings=widget.findChild<SettingsPage*>();QVERIFY(settings);
        settings->auditRequested("磁盘告警","refactor-system-warning",0);
        settings->auditRequested("修改配置","refactor-user-setting");
        QSqlQuery q(DbConn::getInstance()->database());
        QVERIFY(q.exec("SELECT admin_id FROM operation_log WHERE operation_desc='refactor-system-warning' ORDER BY id DESC LIMIT 1"));
        QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);
        QVERIFY(q.exec("SELECT admin_id FROM operation_log WHERE operation_desc='refactor-user-setting' ORDER BY id DESC LIMIT 1"));
        QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),17);
    }

    void componentsOwnTheirStateAndWorkers() {
        MainWidget widget;
        auto playback=widget.findChild<PlaybackPage*>();
        auto monitor=widget.findChild<MonitorPage*>();
        auto settings=widget.findChild<SettingsPage*>();
        auto controller=widget.findChild<MonitoringController*>();
        QVERIFY(playback);QVERIFY(monitor);QVERIFY(settings);QVERIFY(controller);
        auto player=playback->findChild<QMediaPlayer*>();QVERIFY(player);
        QCOMPARE(player->parent(),static_cast<QObject*>(playback));
        QVERIFY(playback->findChildren<QThread*>().isEmpty());
        QVERIFY(monitor->findChildren<QThread*>().isEmpty());
        QVERIFY(settings->findChildren<QThread*>().isEmpty());
        QVERIFY(!controller->findChildren<QThread*>().isEmpty());
        QVERIFY(controller->findChildren<QWidget*>().isEmpty());
        for(auto thread:widget.findChildren<QThread*>()) {
            QObject *owner=thread;
            while(owner && owner!=controller)owner=owner->parent();
            QCOMPARE(owner,static_cast<QObject*>(controller));
        }
    }
    void standalonePlaybackRejectsReplyFromPreviousSession() {
        QTemporaryDir dir;const auto path=dir.filePath("original.mp4");
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("source");file.close();
        PlaybackPage page;page.setAccessEnabled(true);page.show();
        SourceEventRow row;row.path=path;row.channels="1";row.startMs=3500;
        page.receiveSourceEvents({row},0,1);
        auto list=page.findChild<QListWidget*>("sourceEventList");QVERIFY(list);
        QSignalSpy request(&page,&PlaybackPage::sourcePlaybackRequested);
        QVERIFY(QMetaObject::invokeMethod(&page,"play_source_event",Q_ARG(QListWidgetItem*,list->item(0))));
        QCOMPARE(request.count(),1);const auto token=request[0][1].toULongLong();
        page.setAccessEnabled(false);page.setAccessEnabled(true);
        page.receiveSourcePlayback(row,token,{});
        auto player=page.findChild<QMediaPlayer*>();QVERIFY(player->source().isEmpty());
        QCOMPARE(list->count(),0);
    }
    void standalonePagesDoNotOwnMonitoringTasks() {
        MonitorPage monitor;SettingsPage settings;
        QSignalSpy frames(&monitor,&MonitorPage::previewFramePresented);
        monitor.setChannels(ChannelConfigPolicy::defaults());
        monitor.on_capture_state_changed(1,true);
        monitor.display_frame(cv::Mat(60,80,CV_8UC3,cv::Scalar(1,2,3)),1);
        QCOMPARE(frames.count(),0);
        QSignalSpy changed(&settings,&SettingsPage::detectionSaved);
        settings.setDetectionEnabled(true);QCOMPARE(changed.count(),0);
        settings.setAccessEnabled(true);
        QSignalSpy rejected(&settings,&SettingsPage::detectionToggleRejected);
        settings.setDetectionEnabled(true);QCOMPARE(changed.count(),0);QCOMPARE(rejected.count(),1);
        QVERIFY(monitor.findChildren<QThread*>().isEmpty());QVERIFY(settings.findChildren<QThread*>().isEmpty());
    }
    void shutdownIsIdempotentAndMaintenanceIsDisabledForTests() {
        MonitoringController controller;
        controller.setChannels(ChannelConfigPolicy::defaults());controller.initialize();
        controller.shutdown();controller.shutdown();QVERIFY(controller.findChildren<QThread*>().isEmpty());
        MaintenanceController maintenance;
        QSignalSpy recover(&maintenance,&MaintenanceController::recoveryRequested);
        maintenance.start();QTest::qWait(20);QCOMPARE(recover.count(),0);
        for(auto timer:maintenance.findChildren<QTimer*>())QVERIFY(!timer->isActive());
    }



    void fourDifferentFilesLayeredPerformance_data() {
        QTest::addColumn<QString>("mode");QTest::addColumn<int>("width");
        for(int width:{640,1920}) for(const QString &mode:{QString("preview"),QString("record"),QString("full")}) {
            if(mode=="full" && qEnvironmentVariable("SMART_MONITOR_TEST_MODEL").isEmpty())continue;
            QTest::newRow(qPrintable(QString("%1-%2").arg(mode).arg(width)))<<mode<<width;
        }
    }
    void fourDifferentFilesLayeredPerformance() {
        QFETCH(QString,mode);QFETCH(int,width);const int height=width*9/16;
        QTemporaryDir directory;QVERIFY(directory.isValid());
        const QByteArray previousMode=qgetenv("SMART_MONITOR_PIPELINE");
        const auto cleanup=qScopeGuard([&]{
            if(previousMode.isNull())qunsetenv("SMART_MONITOR_PIPELINE");else qputenv("SMART_MONITOR_PIPELINE",previousMode);
            clearBackgroundTestData();
        });
        qputenv("SMART_MONITOR_PIPELINE",mode.toUtf8());
        QString error;QVERIFY2(prepareBackgroundSources(directory,error),qPrintable(error));
        auto channels=ChannelConfigPolicy::defaults();
        for(auto &channel:channels) {
            channel.sourceType=ChannelSourceType::File;
            channel.filePath=directory.filePath(QString("source-%1.avi").arg(channel.channelId));
            cv::VideoWriter writer(channel.filePath.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),25,cv::Size(width,height));
            QVERIFY(writer.isOpened());
            for(int i=0;i<90;++i)writer.write(cv::Mat(height,width,CV_8UC3,cv::Scalar(channel.channelId*40,i%200,60)));
        }
        ChannelConfigService service;QVERIFY2(service.save(channels,error),qPrintable(error));
        ConfigModel config;
        QVERIFY(config.upsertValue("detection.enabled","0"));
        if(mode=="full") {
            const QString model=qEnvironmentVariable("SMART_MONITOR_TEST_MODEL");
            QVERIFY(QFileInfo::exists(model));
            QVERIFY(config.upsertValue("detection.model_path",model));
        }
        int frames[4]={};int inferred[4]={};int cycles=0;qint64 inferenceMs=0; qint64 maxHeartbeatGap=0;
        {
            MainWidget widget;widget.resize(1280,760);widget.show();widget.setLoggedInUser(17,"admin17");
            connect(&widget,&MainWidget::previewFramePresented,[&](int channel){++frames[channel-1];});
            connect(&widget,&MainWidget::detectionCycleCompleted,[&](int channel,qint64 ms){++cycles;++inferred[channel-1];inferenceMs+=ms;});
            if(mode=="full") {
                auto toggle=widget.findChild<QCheckBox*>("globalDetectionSwitch");QVERIFY(toggle);
                toggle->click();QVERIFY(toggle->isChecked());
                QString saved;bool found=false;QVERIFY(config.findValue("detection.enabled",saved,found));
                QVERIFY(found);QCOMPARE(saved,QString("1"));
            }
            QVERIFY(QMetaObject::invokeMethod(widget.findChild<QObject*>("monitoringController"),"start_configured_previews"));
            QTest::qWait(1500);
            for(auto &frame:frames)frame=0;
            QElapsedTimer elapsed;elapsed.start();qint64 lastBeat=0;
            QTimer heartbeat;heartbeat.setInterval(20);
            connect(&heartbeat,&QTimer::timeout,[&]{auto now=elapsed.elapsed();maxHeartbeatGap=qMax(maxHeartbeatGap,now-lastBeat);lastBeat=now;});
            heartbeat.start();QTest::qWait(4000);heartbeat.stop();
            for(int channel=0;channel<4;++channel) {
                qInfo().noquote()<<QString("LAYER %1 channel=%2 presented=%3 fps=%4").arg(mode).arg(channel+1).arg(frames[channel]).arg(frames[channel]*1000.0/elapsed.elapsed(),0,'f',1);
                QVERIFY2(frames[channel]>=72,"Four-channel preview must sustain at least 18 presented FPS on 25 FPS fixtures");
            }
            qInfo()<<"LAYER"<<mode<<"heartbeatMaxMs"<<maxHeartbeatGap<<"inferenceCycles"<<cycles<<"inferenceTotalMs"<<inferenceMs;
            QVERIFY(maxHeartbeatGap<300);
            if(mode=="full") {
                for(int channel=0;channel<4;++channel)QVERIFY(inferred[channel]>0);
                widget.findChild<QCheckBox*>("globalDetectionSwitch")->click();
                const int stoppedCycles=cycles;QTest::qWait(600);QCOMPARE(cycles,stoppedCycles);
                QString saved;bool found=false;QVERIFY(config.findValue("detection.enabled",saved,found));QCOMPARE(saved,QString("0"));
            }
            const auto capture=qEnvironmentVariable("SMART_MONITOR_UI_CAPTURE_DIR");
            if(!capture.isEmpty()){QDir().mkpath(capture);widget.grab().save(QDir(capture).filePath(QString("four-%1-%2.png").arg(mode).arg(width)));}
        }
        const auto recordings=QDir(directory.filePath("recordings")).entryList({"*.mp4"},QDir::Files);
        QCOMPARE(recordings.size(),0); // File sources are indexed, never re-encoded.

    }
    void eventSelectionStartsPlaybackWithoutDetails() {
        QTemporaryDir dir;const QString path=dir.filePath("event.avi");
        cv::VideoWriter writer(path.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),25,cv::Size(64,48));
        QVERIFY(writer.isOpened());for(int i=0;i<50;++i)writer.write(cv::Mat(48,64,CV_8UC3,cv::Scalar(20,40,80)));writer.release();
        QSqlQuery query(DbConn::getInstance()->database());
        const auto cleanup=qScopeGuard([&]{query.exec("DELETE FROM monitor_event WHERE event_key='selection-event'");query.exec("DELETE FROM video WHERE id=66001");});
        query.prepare("INSERT INTO video(id,video_name,video_path,channel_id,start_time,end_time,file_size,video_type) VALUES(66001,'event',:path,1,:start,:end,100,'event')");
        query.bindValue(":path",path);query.bindValue(":start",QDateTime::currentDateTime().addSecs(-10));query.bindValue(":end",QDateTime::currentDateTime());QVERIFY(query.exec());
        VideoModel eventModel;QVERIFY(eventModel.createEvent("selection-event",1,QDateTime::currentDateTime(),path,0.9));
        MainWidget widget;widget.setLoggedInUser(17,"admin17");widget.show();
        QVERIFY(QMetaObject::invokeMethod(&widget,"show_playback_page"));
        auto list=widget.findChild<QListWidget*>("recordList");QVERIFY(list);
        QListWidgetItem *event=nullptr;
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toInt()==66001)event=list->item(i);
        QVERIFY(event);QVERIFY(event->data(Qt::UserRole+2).toBool());QVERIFY(!event->toolTip().isEmpty());
        list->setCurrentItem(event);
        auto player=widget.findChild<QMediaPlayer*>();QVERIFY(player);
        QTRY_COMPARE_WITH_TIMEOUT(player->source(),QUrl::fromLocalFile(path),2000);
        QTRY_COMPARE_WITH_TIMEOUT(player->playbackState(),QMediaPlayer::PlayingState,2000);
    }
    void reviewedLayoutAndGlobalSwitch_data() {
        QTest::addColumn<int>("windowWidth");
        QTest::newRow("1280")<<1280;QTest::newRow("1600")<<1600;
    }
    void reviewedLayoutAndGlobalSwitch() {
        QFETCH(int,windowWidth);
        MainWidget widget;widget.setLoggedInUser(17,"admin17");widget.resize(windowWidth,760);widget.show();
        QTest::qWait(50);
        auto toggle=widget.findChild<QCheckBox*>("globalDetectionSwitch");
        QVERIFY(toggle);QVERIFY(toggle->isVisible());QVERIFY(toggle->isEnabled());
        auto left=widget.findChild<QWidget*>("devicePanel");
        auto right=widget.findChild<QWidget*>("monitorPanel");
        const double previewRatio=double(left->width())/(left->width()+right->width());
        QVERIFY2(previewRatio>=.20 && previewRatio<=.30,qPrintable(QString::number(previewRatio)));
        QVERIFY(QMetaObject::invokeMethod(&widget,"show_playback_page"));QTest::qWait(50);
        left=widget.findChild<QWidget*>("recordFilterPanel");
        right=widget.findChild<QWidget*>("playbackDisplayPanel");
        const double playbackRatio=double(left->width())/(left->width()+right->width());
        QVERIFY2(playbackRatio>=.20 && playbackRatio<=.305,qPrintable(QString::number(playbackRatio)));
        auto start=widget.findChild<QDateTimeEdit*>("playbackStartTime");
        auto end=widget.findChild<QDateTimeEdit*>("playbackEndTime");
        QVERIFY(start->mapTo(&widget,QPoint()).y()<end->mapTo(&widget,QPoint()).y());
        auto capture=qEnvironmentVariable("SMART_MONITOR_UI_CAPTURE_DIR");
        if(!capture.isEmpty()) {QDir().mkpath(capture);widget.grab().save(QDir(capture).filePath("review-playback.png"));}
    }
    void guestStartsBackgroundWorkersAndRejectsFrames() {
        MainWidget widget;widget.show();QTest::qWait(20);
        const auto workers = widget.findChildren<QThread*>();
        QCOMPARE(workers.size(),10);
        for(auto thread:workers) QVERIFY(thread->isRunning());
        QVERIFY(!widget.findChild<QPushButton*>("startPreviewButton"));
        cv::Mat frame(120,160,CV_8UC3,cv::Scalar(0,200,0));
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"display_frame",Q_ARG(cv::Mat,frame),Q_ARG(int,1)));
        for(auto label:widget.findChildren<QLabel*>("videoWindow")) QVERIFY(label->pixmap().isNull());
    }
    void logoutKeepsWorkersClearsPreviewAndAllowsLoginAgain() {
        MainWidget widget;widget.show();QTest::qWait(20);
        const auto workers = widget.findChildren<QThread*>();
        QCOMPARE(workers.size(),10);
        widget.setLoggedInUser(17,"admin17");
        QCOMPARE(widget.findChildren<QThread*>(),workers);
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QObject*>("monitoringController"),"on_capture_state_changed",Q_ARG(int,1),Q_ARG(bool,true)));
        cv::Mat frame(120,160,CV_8UC3,cv::Scalar(0,200,0));
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"display_frame",Q_ARG(cv::Mat,frame),Q_ARG(int,1)));
        QVERIFY(!widget.findChildren<QLabel*>("videoWindow").first()->pixmap().isNull());
        QSignalSpy loginRequest(&widget,&MainWidget::login_requested);
        // 兼容修复前的退出提示，防止 modal 对话框挂起回归。
        QTimer closer;connect(&closer,&QTimer::timeout,[] {for(auto window:QApplication::topLevelWidgets()) if(auto box=qobject_cast<QMessageBox*>(window)) box->accept();});closer.start(20);
        widget.findChild<QPushButton*>("loginButton")->click();closer.stop();
        QCOMPARE(loginRequest.count(),1);
        QCOMPARE(widget.findChildren<QThread*>(),workers);
        for(auto thread:workers) QVERIFY(thread->isRunning());
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"display_frame",Q_ARG(cv::Mat,frame),Q_ARG(int,1)));
        for(auto label:widget.findChildren<QLabel*>("videoWindow")) QVERIFY(label->pixmap().isNull());
        widget.setLoggedInUser(17,"admin17");QTest::qWait(20);
        QCOMPARE(widget.findChildren<QThread*>(),workers);
        for(auto thread:widget.findChildren<QThread*>()) QVERIFY(thread->isRunning());
        for(auto window:widget.findChildren<QLabel*>("videoWindow")) QVERIFY(!window->text().contains("请先登录"));
    }
    void startupRoutesToLoginWindow() {
        auto manager=WindowManager::getInstance();manager->showMainWindow();QTest::qWait(20);
        bool mainVisible=false,loginVisible=false;
        for(auto window:QApplication::topLevelWidgets()) {
            if(qobject_cast<MainWidget*>(window))mainVisible=mainVisible||window->isVisible();
            if(qobject_cast<LoginWidget*>(window))loginVisible=loginVisible||window->isVisible();
        }
        WindowManager::releaseInstance();
        QVERIFY(!mainVisible);QVERIFY(loginVisible);
    }
    void hiddenChannelDoesNotRenderPixmap() {
        MainWidget widget;widget.show();widget.setLoggedInUser(17,"admin17");
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"show_single_window"));
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QObject*>("monitoringController"),"on_capture_state_changed",Q_ARG(int,2),Q_ARG(bool,true)));
        cv::Mat frame(1080,1920,CV_8UC3,cv::Scalar(0,200,0));
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"display_frame",Q_ARG(cv::Mat,frame),Q_ARG(int,2)));
        QVERIFY(widget.findChildren<QLabel*>("videoWindow").at(1)->pixmap().isNull());
    }

    void settingsRemainReadableAtSmallWindowSizes_data() {
        QTest::addColumn<QSize>("windowSize");
        QTest::newRow("minimum")<<QSize(960,600);
        QTest::newRow("default")<<QSize(1280,760);
    }
    void settingsRemainReadableAtSmallWindowSizes() {
        QFETCH(QSize,windowSize);
        MainWidget widget;widget.setLoggedInUser(17,"admin17");
        widget.resize(windowSize);widget.show();
        QVERIFY(QMetaObject::invokeMethod(&widget,"show_setting_page"));QTest::qWait(20);
        auto page=widget.findChild<QWidget*>("settingPage");
        for(auto input:page->findChildren<QLineEdit*>()) {
            // SpinBox 已在外壳分配边距，内部编辑器只要求完整容纳字体高度。
            const int padding=qobject_cast<QAbstractSpinBox*>(input->parentWidget())?0:6;
            QVERIFY2(input->height()>=input->fontMetrics().height()+padding,
                     qPrintable(QString("%1 height=%2 font=%3").arg(input->objectName()).arg(input->height()).arg(input->fontMetrics().height())));
        }
        auto scroll=widget.findChild<QScrollArea*>("settingsScrollArea");QVERIFY(scroll);
        QVERIFY(scroll->verticalScrollBar()->maximum()>0);
        auto save=widget.findChild<QPushButton*>("saveChannelConfigButton");
        scroll->ensureWidgetVisible(save);QTest::qWait(20);
        QVERIFY(!save->visibleRegion().isEmpty());
    }

    void filePreviewContinuesAcrossLoginWithoutRecordingCopies() {
        QTemporaryDir directory;QString error;
        const auto cleanup=qScopeGuard([]{clearBackgroundTestData();});
        QVERIFY(prepareBackgroundSources(directory,error));
        const QStringList original=QDir(directory.path()).entryList({"*.avi"});
        {
            MainWidget widget;widget.show();QVERIFY(QMetaObject::invokeMethod(widget.findChild<QObject*>("monitoringController"),"start_configured_previews"));
            QTest::qWait(400);QVERIFY(allPreviewFramesAreHidden(widget));
            widget.setLoggedInUser(17,"admin17");auto window=widget.findChildren<QLabel*>("videoWindow").first();
            QTRY_VERIFY_WITH_TIMEOUT(!window->pixmap().isNull(),2000);
            widget.findChild<QPushButton*>("loginButton")->click();QTest::qWait(400);QVERIFY(allPreviewFramesAreHidden(widget));
            widget.setLoggedInUser(17,"admin17");QTRY_VERIFY_WITH_TIMEOUT(!window->pixmap().isNull(),2000);
            // A late legacy detection callback must not re-enable file recording.
            QVERIFY(QMetaObject::invokeMethod(widget.findChild<QObject*>("monitoringController"),"on_person_detected",Q_ARG(int,2),Q_ARG(QString,QString("late-file-event")),Q_ARG(QDateTime,QDateTime::currentDateTime()),Q_ARG(double,0.9)));
        }
        QCOMPARE(QDir(directory.filePath("recordings")).entryList({"*.mp4"},QDir::Files).size(),0);
        QCOMPARE(QDir(directory.path()).entryList({"*.avi"}),original);
        QSqlQuery q(DbConn::getInstance()->database());QVERIFY(q.exec("SELECT COUNT(*) FROM video"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);
    }

    void previewColorsAndSnapshotKeepOriginalResolution() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        ConfigModel config;QVERIFY(config.upsertValue("image_dir",dir.path()));
        QSqlQuery sql(DbConn::getInstance()->database());
        const auto cleanup = qScopeGuard([&] {sql.exec("DELETE FROM sys_config WHERE config_key='image_dir'");sql.exec("DELETE FROM feature_image");});
        MainWidget widget;widget.show();widget.setLoggedInUser(17,"admin17");
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QObject*>("monitoringController"),"on_capture_state_changed",Q_ARG(int,1),Q_ARG(bool,true)));
        cv::Mat source(1080,1920,CV_8UC3,cv::Scalar(20,80,220));
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"display_frame",Q_ARG(cv::Mat,source),Q_ARG(int,1)));
        const QString captureDir=qEnvironmentVariable("SMART_MONITOR_UI_CAPTURE_DIR");
        if(!captureDir.isEmpty()) {
            QDir().mkpath(captureDir);
            QTest::qWait(20);widget.grab().save(QDir(captureDir).filePath("main-preview.png"));
            QVERIFY(QMetaObject::invokeMethod(&widget,"show_setting_page"));
            QTest::qWait(20);widget.grab().save(QDir(captureDir).filePath("main-settings.png"));
            QVERIFY(QMetaObject::invokeMethod(&widget,"show_monitor_page"));
        }
        const QImage preview=widget.findChildren<QLabel*>("videoWindow").first()->pixmap().toImage();
        QVERIFY(preview.width()<source.cols);
        QCOMPARE(preview.pixelColor(preview.width()/2,preview.height()/2),QColor(220,80,20));
        // 放开生产者的引用，截图仍须保留原帧，并且不能保存缩略图。
        source.release();
        widget.findChild<QPushButton*>("previewSnapshotButton")->click();
        QVERIFY(sql.exec("SELECT image_path FROM feature_image ORDER BY id DESC LIMIT 1"));
        QVERIFY(sql.next());
        const QImage snapshot(sql.value(0).toString());
        QCOMPARE(snapshot.size(),QSize(1920,1080));
        QVERIFY(qAbs(snapshot.pixelColor(960,540).red()-220)<=3);
    }

    void failedPlaybackStopsOldVideoAndSuccessClearsError() {
        QTemporaryDir dir;const QString path=dir.filePath("playback.avi");
        cv::VideoWriter writer(path.toStdString(),cv::VideoWriter::fourcc('M','J','P','G'),10,cv::Size(32,24));
        QVERIFY(writer.isOpened());for(int i=0;i<30;++i)writer.write(cv::Mat(24,32,CV_8UC3,cv::Scalar(i*5,40,80)));writer.release();
        QSqlQuery query(DbConn::getInstance()->database());
        query.prepare("INSERT INTO video(id,video_name,video_path,channel_id,start_time,end_time,file_size) VALUES(65001,'test',:path,1,:start,:end,100)");
        query.bindValue(":path",path);query.bindValue(":start",QDateTime::currentDateTime().addSecs(-10));query.bindValue(":end",QDateTime::currentDateTime());QVERIFY(query.exec());
        MainWidget widget;widget.setLoggedInUser(17,"admin17");widget.show();
        auto list=widget.findChild<QListWidget*>("recordList");QVERIFY(list);list->clear();
        auto valid=new QListWidgetItem("valid",list);valid->setData(Qt::UserRole+1,65001);
        auto missing=new QListWidgetItem("missing",list);missing->setData(Qt::UserRole+1,999999);
        auto player=widget.findChild<QMediaPlayer*>();QVERIFY(player);
        list->setCurrentItem(valid);QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("playbackPage"),"play_selected_record"));
        QTRY_VERIFY_WITH_TIMEOUT(player->duration()>0,3000);
        QCOMPARE(player->source(),QUrl::fromLocalFile(path));
        list->setCurrentItem(missing);QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("playbackPage"),"play_selected_record"));
        QCOMPARE(player->playbackState(),QMediaPlayer::StoppedState);
        QVERIFY(player->source().isEmpty());
        list->setCurrentItem(valid);QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("playbackPage"),"play_selected_record"));
        QCOMPARE(player->source(),QUrl::fromLocalFile(path));
        QTRY_VERIFY_WITH_TIMEOUT(player->duration()>0,3000);
        QCOMPARE(player->error(),QMediaPlayer::NoError);
        auto page=widget.findChild<QWidget*>("playbackPage");
        for(auto label:page->findChildren<QLabel*>()) QVERIFY(!label->text().contains(QString::fromUtf8("录像记录不存在")));
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("playbackPage"),"stop_playback"));
        QVERIFY(query.exec("DELETE FROM video WHERE id=65001"));
    }

    void backgroundChannelRefreshDoesNotReselectOrChangeSingleView() {
        MainWidget widget;widget.show();QTest::qWait(20);
        auto list=widget.findChild<QListWidget*>("deviceList");QVERIFY(list);
        list->setCurrentRow(2);QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"show_single_window"));
        QSignalSpy selectionChanges(list,&QListWidget::currentRowChanged);
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QObject*>("monitoringController"),"on_capture_state_changed",Q_ARG(int,1),Q_ARG(bool,true)));
        QCOMPARE(list->currentRow(),2);
        QCOMPARE(selectionChanges.count(),0);
        auto windows=widget.findChildren<QLabel*>("videoWindow");
        for(int i=0;i<4;++i) QCOMPARE(!windows[i]->isHidden(),i==2);
    }
    void missingPlaybackRecordReportsErrorOnPlaybackPage() {
        MainWidget widget;widget.setLoggedInUser(17,"admin17");widget.show();
        QVERIFY(QMetaObject::invokeMethod(&widget,"show_playback_page"));
        auto list=widget.findChild<QListWidget*>("recordList");QVERIFY(list);
        auto item=new QListWidgetItem("missing",list);item->setData(Qt::UserRole+1,999999);list->setCurrentItem(item);
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("playbackPage"),"play_selected_record"));
        auto page=widget.findChild<QWidget*>("playbackPage");QVERIFY(page);
        bool visibleError=false;
        for(auto label:page->findChildren<QLabel*>()) if(label->isVisible()&&label->text().contains(QString::fromUtf8("录像记录不存在"))) visibleError=true;
        QVERIFY2(visibleError,"Playback error must be visible on the current page");
    }

    void singleViewFollowsMouseChannelSelection() {
        MainWidget widget; widget.show(); QTest::qWait(20);
        auto list=widget.findChild<QListWidget*>("deviceList"); QVERIFY(list);
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"show_single_window"));
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(2)).center());
        auto windows=widget.findChildren<QLabel*>("videoWindow");
        QCOMPARE(windows.size(),4);
        for(int i=0;i<4;++i) QCOMPARE(!windows[i]->isHidden(),i==2);
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"show_multi_window"));
        for(auto window:windows) QVERIFY(!window->isHidden());
    }
    void singleViewFollowsKeyboardChannelSelection() {
        MainWidget widget; widget.show(); QTest::qWait(20);
        auto list=widget.findChild<QListWidget*>("deviceList"); QVERIFY(list);
        list->setCurrentRow(0); list->setFocus();
        QVERIFY(QMetaObject::invokeMethod(widget.findChild<QWidget*>("contentArea"),"show_single_window"));
        QTest::keyClick(list,Qt::Key_Down);
        QCOMPARE(list->currentRow(),1);
        auto windows=widget.findChildren<QLabel*>("videoWindow");
        for(int i=0;i<4;++i) QCOMPARE(!windows[i]->isHidden(),i==1);
    }
    void configurationActionsHaveDistinctControls() {
        MainWidget widget;
        QCOMPARE(widget.findChildren<QPushButton*>("saveChannelConfigButton").size(),1);
        QVERIFY(widget.findChild<QPushButton*>("saveRecordingSettingsButton"));
        QVERIFY(widget.findChild<QPushButton*>("saveDetectionSettingsButton"));
    }

    void managementPagesRequireLoginAndExposeRealControls() {
        MainWidget widget;
        auto imageButton=widget.findChild<QPushButton*>("imagesNavigation");
        auto logButton=widget.findChild<QPushButton*>("logsNavigation");
        QVERIFY(imageButton); QVERIFY(logButton);
        QVERIFY(!imageButton->isEnabled()); QVERIFY(!logButton->isEnabled());
        QVERIFY(widget.findChild<QLineEdit*>("imageDirectoryEdit"));
        widget.setLoggedInUser(17,"admin17");
        QVERIFY(imageButton->isEnabled()); QVERIFY(logButton->isEnabled());
        imageButton->click();
        QVERIFY(widget.findChild<QWidget*>("imagePage")->isEnabled());
        logButton->click();
        QVERIFY(widget.findChild<QWidget*>("operationLogPage")->isEnabled());
    }

    void initTestCase() {
        cv::setNumThreads(qBound(1,QThread::idealThreadCount(),4)); // 与 main.cpp 一致。
        QStandardPaths::setTestModeEnabled(true);
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        QVERIFY(initializeTestDatabase());
        // 本测试的事件链扩展只存在于内存库，不执行生产数据库迁移。
        QSqlQuery query(DbConn::getInstance()->database());
        const QStringList statements={
            "ALTER TABLE video ADD COLUMN video_type TEXT DEFAULT 'normal'",
            "ALTER TABLE video ADD COLUMN exception_id INTEGER",
            "CREATE TABLE exception_log(id INTEGER PRIMARY KEY AUTOINCREMENT,channel_id INTEGER,event_type TEXT,event_time TEXT,event_end_time TEXT,confidence REAL,event_status TEXT,event_desc TEXT,related_video_path TEXT)",
            "CREATE TABLE event_media_link(event_key TEXT PRIMARY KEY,exception_id INTEGER,video_id INTEGER)"
        };
        for(const auto &statement:statements)
            QVERIFY2(query.exec(statement),qPrintable(query.lastError().text()));
    }
    void cleanupTestCase() { DbConn::releaseDbConn(); }
    // 验证监控页初始显示四个可见视频窗口。
    void monitorPageStartsWithFourVisibleChannels();
    // 验证通道操作按钮和四组配置输入控件均已创建。
    void channelControlsAndFourConfigRowsArePresent();
};

void Day3MainWidgetTests::monitorPageStartsWithFourVisibleChannels()
{
    MainWidget widget;
    widget.show();
    QCoreApplication::processEvents();

    const QList<QLabel *> windows =
            widget.findChildren<QLabel *>(QStringLiteral("videoWindow"));
    QCOMPARE(windows.size(), 4);
    for (QLabel *window : windows) {
        QVERIFY(!window->isHidden());
    }
}

void Day3MainWidgetTests::channelControlsAndFourConfigRowsArePresent()
{
    MainWidget widget;

    QListWidget *deviceList =
            widget.findChild<QListWidget *>(QStringLiteral("deviceList"));
    QVERIFY(deviceList != nullptr);
    QCOMPARE(deviceList->count(), 4);

    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("startPreviewButton")) == nullptr);
    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("stopPreviewButton")) == nullptr);
    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("refreshCamerasButton")) != nullptr);
    QVERIFY(widget.findChild<QPushButton *>(
                QStringLiteral("saveChannelConfigButton")) != nullptr);

    for (int channelId = 1; channelId <= 4; ++channelId) {
        QVERIFY(widget.findChild<QLineEdit *>(
                    QStringLiteral("channelNameEdit_%1").arg(channelId))
                != nullptr);
    }
}

QTEST_MAIN(Day3MainWidgetTests)
#include "tst_day3_mainwidget.moc"
