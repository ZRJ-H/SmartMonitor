# Day20 Qt6 视频监控融合 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 在现有Qt监控系统中实现四路采集显示、10秒分段录制和单例数据库入库。

**Architecture:** 保持MVC/QTVC分层。MonitorTask和RecordTask分别在独立线程中完成采集与录制，Video值对象通过排队信号回到主线程，再由VideoService、VideoModel和DbConn单例入库。

**Tech Stack:** C++17、Qt 6.11.2 Widgets/Sql/Test、qmake、MinGW 13.1 64-bit、OpenCV 4.14、MySQL 5.7。

**Spec:** docs/superpowers/specs/2026-09-16-day20-qt6-video-recording-design.md

## Global Constraints

- 工程源目录为 E:/QT/LESSON20/day15_pro。
- 先备份原工程，再在工作副本修改，验证后同步。
- 通道1使用摄像头0，通道2至4运行时选文件。
- 保持DbConn单例和唯一QSqlDatabase连接，SQL只在GUI主线程运行。
- 新增代码必须有解释线程、槽、所有权和清理顺序的中文注释。
- 不破坏登录、导航、设置和录像分页。
- 新增核心逻辑遵循TDD。
- 项目不是Git仓库，不执行提交步骤。

---

### Task 1: 建立可恢复工作副本和Qt6基线

**Files:**
- Copy source: E:/QT/LESSON20/day15_pro
- Working copy: C:/Users/周睿杰/Documents/QT学习/day20_smart_monitor_work
- Backup: E:/QT/LESSON20/day15_pro_backup_before_day20_20260916
- Modify: day15_pro.pro
- Modify: util/verifycodelabel.cpp

**Interfaces:**
- Consumes: 现有Qt5 qmake工程
- Produces: 可由Qt6 qmake解析的工作副本

- [ ] 复制原工程到备份和工作副本，排除debug、release、bin中的旧构建产物。
- [ ] 修改pro为QT += core gui widgets sql、CONFIG += c++17、TARGET = day20_smart_monitor，并include(E:/opencv/opencv_qt6.pri)。
- [ ] 将VerifyCodeLabel中的qrand/qsrand替换为QRandomGenerator::global()->bounded()。
- [ ] 使用Qt6 qmake生成Makefile。

Run:
D:/Develop/QT6/6.11.2/mingw_64/bin/qmake.exe day15_pro.pro

Expected:
qmake exit 0；若编译尚因新增功能缺失失败，不在本任务处理。

### Task 2: Video与RecordingSegment测试先行

**Files:**
- Create: model/video.h
- Create: model/video.cpp
- Create: service/recordingsegment.h
- Create: service/recordingsegment.cpp
- Create: tests/day20_core_tests.pro
- Create: tests/tst_day20_core.cpp

**Interfaces:**
- Produces: Video(int, QString, QString, int, QDateTime, QDateTime, qint64)
- Produces: RecordingSegment::begin(QString, int, QDateTime)
- Produces: RecordingSegment::shouldRoll(qint64 elapsedMs)
- Produces: RecordingSegment::finish(QDateTime, qint64)

- [ ] 编写失败测试：Video能返回名称、路径、通道、开始/结束时间和大小。
- [ ] 编写失败测试：RecordingSegment文件名含Record_、时间和一基通道号。
- [ ] 编写失败测试：9999ms不滚动，10000ms滚动。
- [ ] 运行qmake和make，确认因Video/RecordingSegment尚不存在而失败。

Run:
D:/Develop/QT6/6.11.2/mingw_64/bin/qmake.exe day20_core_tests.pro
D:/Develop/QT6/Tools/mingw1310_64/bin/mingw32-make.exe -f Makefile

Expected:
FAIL，缺少model/video.h或service/recordingsegment.h。

- [ ] 实现最小Video值对象并添加Q_DECLARE_METATYPE(Video)。
- [ ] 实现RecordingSegment的命名、边界和Video组装。
- [ ] 重新构建并运行day20_core_tests.exe。

Expected:
所有Video和RecordingSegment测试PASS。

### Task 3: VideoModel与VideoService入库

**Files:**
- Modify: model/videomodel.h
- Modify: model/videomodel.cpp
- Modify: service/videoservice.h
- Modify: service/videoservice.cpp
- Modify: tests/tst_day20_core.cpp

**Interfaces:**
- Consumes: Video
- Produces: VideoModel::insertVideo(const Video &)
- Produces: VideoService::saveVideo(const Video &, QString &)

- [ ] 编写失败测试：使用真实QMYSQL连接启动事务，插入临时Video后可按唯一路径查询到相同通道、start_time、end_time和file_size，再回滚。
- [ ] 运行测试确认因insertVideo/saveVideo不存在而失败。
- [ ] 实现参数化INSERT，绑定videoName、videoPath、channelId、startTime、endTime、fileSize。
- [ ] 错误时保存last_error_text，Service向上返回。
- [ ] 运行测试，确认插入映射PASS且数据库中不保留测试记录。
- [ ] 运行原分页测试/查询，确认countVideos和loadPage行为不回归。

### Task 4: MonitorTask采集Worker

**Files:**
- Create: service/monitortask.h
- Create: service/monitortask.cpp
- Modify: main.cpp
- Modify: day15_pro.pro
- Modify: tests/tst_day20_core.cpp
- Modify: tests/day20_core_tests.pro

**Interfaces:**
- Produces slots: MonitorTask::start(), MonitorTask::stop()
- Produces signals: sourceOpened(int,int,int,double), frameReady(cv::Mat,int), sourceError(int,QString), finished(int)

- [ ] 编写失败测试：无效视频路径启动后发出sourceError且不发frameReady。
- [ ] 编写失败测试：测试生成的短视频可发出sourceOpened与frameReady。
- [ ] 运行测试确认MonitorTask缺失导致失败。
- [ ] 实现工作线程QTimer驱动读取、FPS回退、文件循环和连续失败保护。
- [ ] 在main.cpp注册cv::Mat与Video元类型。
- [ ] 运行MonitorTask测试并确认PASS。

### Task 5: RecordTask分段录制Worker

**Files:**
- Create: service/recordtask.h
- Create: service/recordtask.cpp
- Modify: day15_pro.pro
- Modify: tests/tst_day20_core.cpp
- Modify: tests/day20_core_tests.pro

**Interfaces:**
- Consumes: cv::Mat、channelId、source FPS
- Produces slots: configureSource(int,double), saveFrame(cv::Mat,int), stop()
- Produces signals: segmentSaved(Video), recordError(int,QString), stopped(int)

- [ ] 编写失败测试：使用50ms测试分段时长和生成帧，RecordTask产生非空视频文件及segmentSaved。
- [ ] 编写失败测试：stop能关闭不足分段时长的有效文件并只发一次segmentSaved。
- [ ] 运行测试确认RecordTask缺失导致失败。
- [ ] 实现每通道Writer、目录创建、H264优先与mp4v回退、边界滚动和幂等停止。
- [ ] 运行测试并确认文件可由VideoCapture重新打开、测试结束后清理临时文件。

### Task 6: MonitorWidget四通道界面和线程生命周期

**Files:**
- Create: view/monitorwidget.h
- Create: view/monitorwidget.cpp
- Modify: view/mainwidget.h
- Modify: view/mainwidget.cpp
- Modify: day15_pro.pro

**Interfaces:**
- Consumes: MonitorTask、RecordTask、VideoService
- Produces: startAllChannels(), stopAllChannels(), displayFrame(cv::Mat,int)
- Produces: recordingInserted()信号供MainWidget刷新回放列表

- [ ] 先写QtTest失败用例：MonitorWidget拥有4个videoWindow对象、3个fileButton对象和single/four/start/stop按钮。
- [ ] 实现UI、文件选择、单/四画面布局和每路状态。
- [ ] 为每个已配置通道创建采集线程和录制线程。
- [ ] 连接frameReady到displayFrame和对应RecordTask::saveFrame。
- [ ] 连接segmentSaved到主线程VideoService::saveVideo。
- [ ] 实现幂等stopAllChannels和析构清理顺序。
- [ ] 将MonitorWidget嵌入MainWidget第0页，删除旧占位视频布局职责。
- [ ] 运行Widget测试（QT_QPA_PLATFORM=offscreen）并确认PASS。

### Task 7: 完整Qt6构建和运行部署

**Files:**
- Modify: day15_pro.pro
- Output: bin/day20_smart_monitor.exe
- Output: bin/Qt6*.dll、platforms、sqldrivers、OpenCV DLL、libmysql.dll

**Interfaces:**
- Consumes: 全部生产代码
- Produces: 可启动的Qt6应用

- [ ] 删除工作副本中的旧Makefile、debug、release和旧目标文件。
- [ ] 运行Qt6 qmake和MinGW完整构建。
- [ ] 修复所有编译、moc和链接错误；每个修复先添加或扩展能复现的测试。
- [ ] 运行全部QtTest。
- [ ] 使用windeployqt部署。
- [ ] 复制libopencv_world4140.dll和opencv_videoio_ffmpeg4140_64.dll。
- [ ] 使用64位libmysql.dll并确认QMYSQL插件可加载。
- [ ] 使用QT_QPA_PLATFORM=offscreen启动5秒，确认无立即崩溃。

Run:
D:/Develop/QT6/6.11.2/mingw_64/bin/qmake.exe day15_pro.pro
D:/Develop/QT6/Tools/mingw1310_64/bin/mingw32-make.exe -f Makefile -j4
bin/day20_core_tests.exe
D:/Develop/QT6/6.11.2/mingw_64/bin/windeployqt.exe bin/day20_smart_monitor.exe

Expected:
构建和测试exit 0，程序能够启动并进入事件循环。

### Task 8: 同步原工程和端到端验证

**Files:**
- Sync intended source/config/docs/tests from working copy to E:/QT/LESSON20/day15_pro
- Preserve: day15_pro.pro.user、数据库内容、用户未修改文件

**Interfaces:**
- Produces: 用户实际工程中的完成版本

- [ ] 比较工作副本和原工程，只同步计划列出的源码、配置、测试和设计文档。
- [ ] 在原工程目录重新运行Qt6 qmake、make和QtTest。
- [ ] 查询QSqlDatabase::drivers确认QMYSQL存在。
- [ ] 生成三个短测试视频供运行时选择。
- [ ] 启动应用，验证登录、四路启动、单/四画面和10秒分段。
- [ ] 查询video表，确认新记录通道、路径、时间和大小。
- [ ] 录制中停止，确认最后分段入库且线程退出。
- [ ] 更新第20章理论笔记，记录最终真实文件和实现差异。
