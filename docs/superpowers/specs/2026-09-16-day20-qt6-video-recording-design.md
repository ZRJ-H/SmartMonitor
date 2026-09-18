# Day20 Qt6 视频实时处理与数据库融合设计

## 目标与约束

在 E:/QT/LESSON20/day15_pro 的现有登录、主窗口、录像分页和数据库代码上实现第20章综合案例。

- Qt 6.11.2 MinGW 64-bit，OpenCV 4.14。
- 通道1使用摄像头0；通道2至4运行时选择视频文件。
- 四路独立采集、显示和录制，支持单画面与2×2四画面。
- 每10秒生成MP4分段，停止时关闭并登记最后不足10秒的分段。
- 每个分段封装成Video，经现有MVC/QTVC分层写入MySQL。
- 保留登录、导航、设置和录像分页。
- 新代码使用清晰中文注释说明线程、槽、所有权和停止顺序。
- 保留DbConn单例，应用内只有一个QSqlDatabase连接，并且只在GUI主线程使用。
- 项目不是Git仓库，不创建worktree。

## MVC/QTVC架构

View：
- MainWidget、MonitorWidget、LoginWidget。
- 只负责控件、用户操作和结果展示。

Controller、Task、Service：
- WindowManager负责窗口切换。
- MonitorTask负责单通道采集。
- RecordTask负责单通道分段写入。
- VideoService负责录像入库和原有分页业务。

Model：
- Video是录像元数据值对象。
- VideoModel负责video表SQL。
- UserModel保留登录数据访问。
- DbConn是唯一数据库连接入口。

数据流：

MonitorTask工作线程
 -> frameReady(Mat, channelId)
 -> MonitorWidget主线程显示
 -> RecordTask录制线程
 -> segmentSaved(Video)
 -> VideoService主线程
 -> VideoModel
 -> DbConn单例
 -> MySQL

录制线程不直接执行SQL。

## 文件

新增：
- model/video.h、model/video.cpp
- service/monitortask.h、service/monitortask.cpp
- service/recordtask.h、service/recordtask.cpp
- service/recordingsegment.h、service/recordingsegment.cpp
- view/monitorwidget.h、view/monitorwidget.cpp
- tests/day20_core_tests.pro
- tests/tst_day20_core.cpp

修改：
- day15_pro.pro、main.cpp
- model/videomodel.h、model/videomodel.cpp
- service/videoservice.h、service/videoservice.cpp
- util/dbconn.h、util/dbconn.cpp
- util/verifycodelabel.cpp
- view/mainwidget.h、view/mainwidget.cpp

运行时创建bin/records。

## Video与数据库

Video保存id、videoName、videoPath、channelId、startTime、endTime、fileSize，并注册为Qt元类型。

真实video表包含id、video_name、video_path、channel_id、start_time、end_time、file_size、exception_id、create_time。插入只写video_name、video_path、channel_id、start_time、end_time、file_size。create_time继续使用数据库CURRENT_TIMESTAMP；录像开始时间写start_time。

VideoModel新增insertVideo(const Video &video)。
VideoService新增saveVideo(const Video &video, QString &errorText)。

## 采集线程

每个通道拥有独立MonitorTask和QThread。

- MonitorTask在工作线程中创建并打开VideoCapture。
- 使用工作线程QTimer逐帧读取，不使用永久while循环。
- 视频使用自身FPS；无效FPS回退30。
- 文件结束后跳回第0帧循环播放。
- 摄像头连续读取失败后只停止当前通道。
- 跨线程发帧前clone。
- cv::Mat注册为Qt元类型。

MonitorTask槽为start和stop；信号为sourceOpened、frameReady、sourceError、finished。

## MonitorWidget

MonitorWidget替代MainWidget原监控页占位实现，保留顶层导航。

界面包含四路状态、通道2至4选择文件按钮、开始全部、停止全部、单画面、四画面、四个视频QLabel及错误提示。

displayFrame槽在GUI主线程中完成MatToQImage和QLabel更新。工作线程不得访问QWidget。单画面默认通道1，选择其他通道后可切换。取消文件选择不覆盖旧路径；未配置通道只提示，不影响其他通道。

## 录制线程

每个通道拥有独立RecordTask、QThread和VideoWriter。

RecordTask槽为configureSource、saveFrame、stop；信号为segmentSaved、recordError、stopped。

规则：
1. 首帧创建records目录。
2. 使用首帧尺寸、通道FPS和H264打开Writer。
3. 文件名为Record_yyyyMMdd_HHmmss_通道号.mp4。
4. 用QElapsedTimer按真实时间判断10秒边界。
5. 边界时关闭Writer并生成Video。
6. 边界帧写入下一分段，避免丢帧。
7. 停止或关闭窗口时登记不足10秒分段。
8. Writer失败时不入库。
9. 文件存在且大小大于0才生成元数据。

RecordingSegment是纯状态类，负责命名、时间、滚动边界和Video组装，供Qt Test测试。

## 单例数据库与生命周期

RecordTask只发出Video值对象，排队信号把Video送到主线程，再由VideoService和VideoModel使用DbConn入库。插入失败时保留视频文件并显示错误。

停止顺序：
1. 停止向录制任务投递新帧。
2. 在采集线程执行MonitorTask::stop。
3. 采集线程quit和wait。
4. 在录制线程执行RecordTask::stop。
5. Writer关闭并发出最后segmentSaved。
6. 主线程完成最后入库。
7. 录制线程quit和wait。
8. 销毁任务和线程。

停止逻辑幂等，避免重复登记。

## Qt6构建

day15_pro.pro使用：
- QT += core gui widgets sql
- CONFIG += c++17
- include(E:/opencv/opencv_qt6.pri)
- TARGET = day20_smart_monitor

验证码迁移到QRandomGenerator。本次涉及的连接使用函数指针语法。

构建工具：
- D:/Develop/QT6/6.11.2/mingw_64/bin/qmake.exe
- D:/Develop/QT6/Tools/mingw1310_64/bin/mingw32-make.exe

部署使用windeployqt，并复制libopencv_world4140.dll、opencv_videoio_ffmpeg4140_64.dll、libmysql.dll、平台插件和QMYSQL插件。

## 错误处理

必须在界面和日志报告：摄像头打不开、文件未选择或打不开、帧读取失败、目录创建失败、H264 Writer失败、输出文件为空、数据库/SQL失败、线程停止失败。单通道错误不影响其他通道。

## 测试

Qt Test覆盖：
1. Video元数据。
2. RecordingSegment文件名和通道号。
3. 10秒滚动边界。
4. 不足10秒的停止分段。
5. VideoService分页回归。
6. VideoModel SQL参数一致性。

新增生产逻辑先写失败测试，再实现。

构建与人工验证：
- Qt6 qmake及完整编译通过。
- windeployqt后程序启动，QMYSQL可用。
- 登录并为通道2至4选择文件。
- 四路显示，单/四画面切换。
- 运行超过10秒，四路各自产生文件和数据库记录。
- 录制中关闭窗口，最后分段入库且线程退出。

## 非目标

不实现目标检测、移动侦测、音频、网络摄像头认证、录像清理、连接池、注册、ORM或动态通道数。

## 验收

Qt6完整编译；现有功能不回归；四路独立显示和录制；10秒分段；最后分段安全关闭；有效分段入库；数据库连接唯一且只在主线程；线程安全退出；中文注释清楚；测试通过。
