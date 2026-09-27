#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QTimer>
#include <QCheckBox>
#include <QDirIterator>
#include <QListWidget>
#include <QMediaPlayer>
#include <QCryptographicHash>
#include <QScopeGuard>
#include <QFontDatabase>
#include <QVideoWidget>
#include <QVideoSink>
#include <QVideoFrame>
#include <opencv2/videoio.hpp>
#include "testdatabase.h"
#include "../view/mainwidget.h"
#include "../service/channelconfigpolicy.h"
class GpuFileWorkflowTests: public QObject {
 Q_OBJECT
private slots:
 void realFootageBaseline(){
    const QString source=qEnvironmentVariable("SMART_MONITOR_TEST_VIDEO");
    if(source.isEmpty())QSKIP("Set SMART_MONITOR_TEST_VIDEO for real-footage integration");
    QTemporaryDir dir;QVERIFY(dir.isValid());
    auto db=DbConn::getInstance()->database();QString error;
    auto channels=ChannelConfigPolicy::defaults();
    for(auto &c:channels){c.sourceType=ChannelSourceType::File;c.filePath=source;}
    ChannelConfigService config;QVERIFY(config.save(channels,error));
    ConfigModel settings;QVERIFY(settings.upsertValue("record.directory",dir.filePath("recordings")));
    QVERIFY(settings.upsertValue("image_dir",dir.filePath("images")));
    QVERIFY(settings.upsertValue("record.segment_seconds","10"));
    QVERIFY(settings.upsertValue("detection.enabled","1"));
    QVERIFY(settings.upsertValue("detection.model_path",qEnvironmentVariable("SMART_MONITOR_TEST_MODEL")));
    qputenv("SMART_MONITOR_INDEX_DIR",dir.filePath("index").toUtf8());
    QFile original(source);QVERIFY(original.open(QIODevice::ReadOnly));QCryptographicHash originalHash(QCryptographicHash::Sha256);QVERIFY(originalHash.addData(&original));original.close();
    int frames[4]={};int cycles=0;qint64 maxGap=0;QVector<qint64> gaps;
    const int duration=qEnvironmentVariableIntValue("SMART_MONITOR_PERF_MS")>0?qEnvironmentVariableIntValue("SMART_MONITOR_PERF_MS"):65000;
    {
      MainWidget widget;widget.show();widget.setLoggedInUser(17,"admin17");
      connect(&widget,&MainWidget::previewFramePresented,[&](int ch){++frames[ch-1];});
      connect(&widget,&MainWidget::detectionCycleCompleted,[&](int,qint64){++cycles;});
      QVERIFY(QMetaObject::invokeMethod(&widget,"start_configured_previews"));
      auto sourceList=widget.findChild<QListWidget*>("sourceEventList");QVERIFY(sourceList);
      QElapsedTimer clock;clock.start();qint64 last=0;QTimer pulse;
      connect(&pulse,&QTimer::timeout,[&]{auto now=clock.elapsed();auto gap=now-last;gaps<<gap;maxGap=qMax(maxGap,gap);last=now;});
      pulse.start(20);QTest::qWait(duration);pulse.stop();
      std::sort(gaps.begin(),gaps.end());
      const auto measuredMs=clock.elapsed();
      QVERIFY(sourceList->count()>0);
      QVERIFY(QMetaObject::invokeMethod(&widget,"show_playback_page"));
      auto row=sourceList->item(0)->data(Qt::UserRole).value<SourceEventRow>();
      sourceList->setCurrentRow(0);
      sourceList->itemClicked(sourceList->item(0));
      QVERIFY(QMetaObject::invokeMethod(&widget,"stop_playback"));
      QTest::qWait(500);
      auto player=widget.findChild<QMediaPlayer*>();QVERIFY(player);
      QVERIFY(player->source().isEmpty());
      QVERIFY(QMetaObject::invokeMethod(&widget,"play_selected_record"));
      QTRY_COMPARE_WITH_TIMEOUT(player->source(),QUrl::fromLocalFile(source),5000);
      QTRY_VERIFY_WITH_TIMEOUT(player->duration()>0,5000);
      QCOMPARE(player->error(),QMediaPlayer::NoError);
      QVERIFY(QMetaObject::invokeMethod(&widget,"pause_playback"));
      QCOMPARE(player->playbackState(),QMediaPlayer::PausedState);
      QVERIFY(QMetaObject::invokeMethod(&widget,"resume_playback"));
      QTRY_COMPARE(player->playbackState(),QMediaPlayer::PlayingState);
      QTRY_VERIFY_WITH_TIMEOUT(widget.findChild<QVideoWidget*>()->videoSink()->videoFrame().isValid(),5000);
      const QString capture=qEnvironmentVariable("SMART_MONITOR_UI_CAPTURE_DIR");
      if(!capture.isEmpty()){QDir().mkpath(capture);QTest::qWait(250);widget.grab().save(capture+"/source-event-playback.png");
        const auto decoded=widget.findChild<QVideoWidget*>()->videoSink()->videoFrame().toImage();QVERIFY(!decoded.isNull());decoded.save(capture+"/decoded-original-frame.png");}
      qInfo()<<"REAL_FOOTAGE"<<"durationMs"<<measuredMs<<"frames"<<frames[0]<<frames[1]<<frames[2]<<frames[3]<<"cycles"<<cycles<<"heartbeatMaxMs"<<maxGap<<"heartbeatP95Ms"<<gaps.value(gaps.size()*95/100);
    }
    qint64 bytes=0;int mp4=0,jpg=0;QDirIterator it(dir.path(),QDir::Files,QDirIterator::Subdirectories);
    while(it.hasNext()){const QFileInfo f(it.next());bytes+=f.size();if(f.suffix()=="mp4")++mp4;if(f.suffix()=="jpg")++jpg;}
    QCOMPARE(mp4,0);QVERIFY(jpg>0);
    QVERIFY(original.open(QIODevice::ReadOnly));QCryptographicHash afterHash(QCryptographicHash::Sha256);QVERIFY(afterHash.addData(&original));original.close();QCOMPARE(afterHash.result(),originalHash.result());
    {
      MainWidget restarted;restarted.show();restarted.setLoggedInUser(17,"admin17");QSignalSpy cyclesAfter(&restarted,&MainWidget::detectionCycleCompleted);
      QVERIFY(QMetaObject::invokeMethod(&restarted,"start_configured_previews"));
      QTRY_VERIFY_WITH_TIMEOUT(restarted.findChild<QListWidget*>("sourceEventList")->count()>0,5000);
      QTest::qWait(500);QCOMPARE(cyclesAfter.count(),0);
    }
    int afterJpg=0,afterMp4=0;QDirIterator after(dir.path(),QDir::Files,QDirIterator::Subdirectories);
    while(after.hasNext()){QFileInfo f(after.next());if(f.suffix()=="jpg")++afterJpg;if(f.suffix()=="mp4")++afterMp4;}
    QCOMPARE(afterJpg,jpg);QCOMPARE(afterMp4,0);
    qInfo()<<"REAL_STORAGE"<<"mp4"<<mp4<<"jpg"<<jpg<<"totalBytes"<<bytes;
 }
 void initTestCase(){
    const int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    if(font>=0)QApplication::setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(),10));
    cv::setNumThreads(4);QVERIFY(initializeTestDatabase());
    QSqlQuery q(DbConn::getInstance()->database());
    for(const auto &s:QStringList{
      "CREATE TABLE exception_log(id INTEGER PRIMARY KEY,channel_id INTEGER,event_type TEXT,event_time TEXT,event_end_time TEXT,confidence REAL,event_status TEXT,event_desc TEXT,related_video_path TEXT)",
      "CREATE TABLE event_media_link(event_key TEXT PRIMARY KEY,exception_id INTEGER,video_id INTEGER)",
      "ALTER TABLE video ADD COLUMN video_type TEXT DEFAULT 'normal'",
      "ALTER TABLE video ADD COLUMN exception_id INTEGER"})QVERIFY2(q.exec(s),qPrintable(q.lastError().text()));
 }
};
QTEST_MAIN(GpuFileWorkflowTests)
#include "tst_gpu_file_workflow.moc"
