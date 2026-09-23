# 本项目里的 C++ 与 Qt 写法拆解

每项按“看到的写法 → 展开意思 → 本项目用途 → 易错点”阅读。
这些是候选补课点，不预设你全部没学过。能解释并写出小例子的项目可以跳过。

## 一、先做 8 分钟诊断

| 写法 | 看得懂 | 能写小例子 | 能解释生命周期或失败情况 |
| --- | --- | --- | --- |
| const 引用 / 输出引用 / 返回 bool | □ | □ | □ |
| 构造初始化列表 / explicit / override | □ | □ | □ |
| auto / 范围 for / enum class | □ | □ | □ |
| lambda 捕获 / 函数指针 connect | □ | □ | □ |
| QObject 父子所有权 / 智能指针 | □ | □ | □ |
| Worker 与队列调用 / 元类型 | □ | □ | □ |
| 事务 / 参数绑定 / 依赖注入 | □ | □ | □ |
| 异步回调 / 原子文件 / 幂等 | □ | □ | □ |

勾选“看得懂”后要能预测输出；只觉得眼熟暂不算通过。

## 二、const、引用和输出参数

入口：[service/systemconfigservice.h](C:/QtProjects/day20_smart_monitor/service/systemconfigservice.h:1)。

~~~cpp
bool load(SystemConfig &config, QString &errorText) const;
bool save(const SystemConfig &config, QString &errorText);
~~~

- `SystemConfig &config`：现有对象的别名，函数可以写入它，调用者获得加载结果。
- `const SystemConfig &config`：借用对象，禁止通过这个引用修改它；不是创建副本。
- `QString &errorText`：通过引用返回错误说明，bool 表示成功/失败。
- 函数末尾 `const`：该成员函数不能直接改动普通成员状态；它不表示“数据库绝不会变化”，更不是线程安全保证。

当前 load 先填临时 loaded，校验成功才赋给 config，避免读到半套错误状态。
练习：解释失败时继续使用旧 config 与拿到半套新 config 有何差别。

## 三、构造初始化列表、explicit 和 override

入口：[service/recordtask.cpp](C:/QtProjects/day20_smart_monitor/service/recordtask.cpp:7)、[view/videolabel.h](C:/QtProjects/day20_smart_monitor/view/videolabel.h:1)。

~~~cpp
RecordTask::RecordTask(QObject *parent)
    : QObject(parent), channel_id_(0), fps_(25.0),
      segment_seconds_(10), enabled_(false) { /* 其余初始化见源码 */ }
~~~

冒号后是初始化对象及其成员，不是“进入函数后再赋值”。
真正初始化顺序由头文件中成员的声明次序决定，不由这里书写先后决定。
析构函数在对象结束时做收尾，本项目 RecordTask 析构还会调用 stop。

`explicit` 防止某些单参数构造函数被意外用作隐式转换。
`override` 让编译器检查你是否真的重写了基类虚函数。
VideoLabel 重写 sizeHint，避免视频像素尺寸不断影响布局；这属于 GUI 尺寸策略，不是视频算法。

## 四、auto、范围 for、enum class

入口：[service/authsession.h](C:/QtProjects/day20_smart_monitor/service/authsession.h:1)、[service/channelconfigservice.cpp](C:/QtProjects/day20_smart_monitor/service/channelconfigservice.cpp:72)。

~~~cpp
for (const ChannelConfig &config : configs) {
    // 逐个只读访问通道配置
}
auto db = DbConn::getInstance()->database();
~~~

`auto` 由初始化表达式推导静态类型，不是动态类型。
`const auto &` 常用于只读遍历避免元素复制；`auto` 可能复制容器元素。
QSqlDatabase 复制的是连接句柄，不能把复制句柄理解成新建独立数据库连接。

`enum class AuthPermission` 是限定作用域的枚举，用 `AuthPermission::Playback` 指定权限。
比散落的数字更容易看出意图，也避免不同枚举项随意混用。

## 五、最值得逐字读的 lambda

入口：[view/mainwidget.cpp](C:/QtProjects/day20_smart_monitor/view/mainwidget.cpp:1404)。下段按实际代码提取关键调用：

~~~cpp
QMetaObject::invokeMethod(
    task,
    [task, config, interval=capture_interval_value_]() {
        task->configureCaptureInterval(interval);
        task->startCapture(config);
    },
    Qt::QueuedConnection);
~~~

- 方括号：捕获列表，说明稍后执行时需要带上哪些外部变量。
- `task`：复制指针值；不会复制 Worker，不会延长 Worker 的生命。
- `config`：复制配置对象，让稍后执行的任务使用这一刻的配置快照。
- `interval=...`：初始化捕获，新建一个叫 interval 的捕获成员。
- `()`：这个可调用对象本次不接收额外参数。
- `{}`：稍后实际执行的函数体。
- invokeMethod 的接收对象决定这个队列调用投递到哪里。

危险对比（教学反例，不要改进工程）：

~~~cpp
auto schedule = [&config] { /* 稍后使用 config */ };
~~~

如果 config 是已经返回的函数里的局部变量，引用捕获可能悬空。
`[this]` 同样只保存指针，不是复制整个窗口。connect 指定接收上下文可以在上下文销毁时断开连接；
这与“任意 lambda 都天然安全”不是一回事。
练习：分别解释复制 config、复制 task、复制 shared_ptr 的区别。

## 六、connect 的成员函数指针不是调用表达式

入口：[view/mainwidget.cpp](C:/QtProjects/day20_smart_monitor/view/mainwidget.cpp:969)。

`&MonitorTask::frameReady` 指向类的成员函数；`task->startCapture(config)` 则立即调用函数。
新式 connect 可以做类型检查。普通成员函数也可作为新式 connect 的接收函数；
按字符串名称的元对象调用则要求相应方法可被元对象系统识别。

`Q_OBJECT` 让 moc 为 QObject 子类生成信号槽等元对象支持。
“undefined reference to vtable”不一定是忘了写析构函数，也可能是 moc 或构建配置缺失。
排查顺序：类声明 → .pro/HEADERS/SOURCES → qmake → moc → 编译/链接日志。

## 七、线程归属、队列和普通调用

`moveToThread` 改变 QObject 的线程归属，不会把每次普通 C++ 调用变成后台任务。
本工程用 QThread 运行事件循环，用 Worker 放具体工作。
QThread 对象本身通常仍在创建它的主线程，不要把它与其管理的运行线程混为一谈。

| 调用方式 | 本项目的含义 |
| --- | --- |
| task->method() | 在调用者线程里普通同步调用 |
| QueuedConnection | 排队，接收线程事件循环处理时执行 |
| BlockingQueuedConnection | 排队并等待对方执行结束；不应对同一线程这样调用 |
| thread->quit() / wait() | 请求结束事件循环 / 等待线程结束；不是强行中断卡住的底层读取 |

当前退出流程使用阻塞调用等待录像封口，遇设备读取长期阻塞的行为仍需现场测试。
这不是鼓励以后到处使用阻塞调用。

## 八、所有权与智能指针

入口：[view/imagepage.h](C:/QtProjects/day20_smart_monitor/view/imagepage.h:1)、[view/imagepage.cpp](C:/QtProjects/day20_smart_monitor/view/imagepage.cpp:353)。

~~~cpp
std::unique_ptr<ImageService> imageService;
auto index = std::make_shared<int>(row);
~~~

ImageService 是普通 C++ 服务，unique_ptr 表示唯一拥有它，拥有者销毁时自动释放。
查看器多个回调需要共享“当前图片索引”，shared_ptr 让该 int 活到最后一个共享持有者消失。
不要只因为“智能”就所有地方换 shared_ptr；循环持有仍会造成泄漏。

QObject 控件则常用父子树：`new QPushButton(this)` 的父对象负责其销毁。
本工程无父对象 Worker 要移到其他线程，在线程 finished 后通过 deleteLater 回收。
不要让 QObject 父对象和一个普通智能指针同时独立拥有同一对象。

## 九、cv::Mat、QImage 和“看起来复制了”

入口：[service/monitortask.cpp](C:/QtProjects/day20_smart_monitor/service/monitortask.cpp:203)、
[view/mainwidget.cpp](C:/QtProjects/day20_smart_monitor/view/mainwidget.cpp:1448)。

cv::Mat 复制通常共享底层像素；clone 才创建独立数据。
用 `rgb.data` 构造 QImage 的这类写法借用现有缓冲区，局部 Mat 结束后不能继续依赖它。
当前代码用 copy 脱离外部缓冲区。

需要区分四件事：对象值复制、指针复制、隐式共享、深复制。
qRegisterMetaType 解决 Qt 如何处理自定义类型的队列参数；
它不会替你自动把 cv::Mat 共享缓冲区变成独立像素。

## 十、事务与文件原子写不是同一件事

入口：[model/systemconfigmodel.cpp](C:/QtProjects/day20_smart_monitor/model/systemconfigmodel.cpp:1)、[service/mediarecoveryservice.cpp](C:/QtProjects/day20_smart_monitor/service/mediarecoveryservice.cpp:1)。

`prepare` 加 `bindValue` 将参数与 SQL 结构分开；不要拼接用户输入构造 SQL。
transaction/commit/rollback 让一组数据库更改整体成功或失败。
它不能回滚已经写到磁盘的 JPG，也不能保证 MP4 一定可播放。

QSaveFile 写临时内容，再在 commit 时提交最终文件；这里只保证该清单写入的原子替换。
当前恢复流程：先写清单 → 尝试 SQL/图片 → 成功删清单。
程序崩溃后重做时不能重复产生数据，这个性质叫“幂等”。

## 十一、依赖注入：测试为什么不用真实库

入口：[model/videomodel.h](C:/QtProjects/day20_smart_monitor/model/videomodel.h:1)、[tests/tst_video_service.cpp](C:/QtProjects/day20_smart_monitor/tests/tst_video_service.cpp:1)。

构造函数接收 QSqlDatabase，调用者可以传实际 MySQL 或测试 SQLite。
这叫把依赖从外部传入，目的是让同一逻辑可以独立验证。
当前工程只部分采用，ChannelConfigService 等仍通过 DbConn 单例取连接。
因此不要把它描述为“完全无全局依赖的架构”。

## 十二、读测试的三个动作

先看准备了什么数据，再看调用哪个业务，最后看断言了什么结果。
QVERIFY 检查真假，QCOMPARE 比较实际与预期，QSignalSpy 收集信号及参数。
QTRY 类断言会等待异步条件；随意写很长的 sleep 会掩盖问题。

练习：在 recordtask_event_tests 中找到“MP4 能打开”和“实际时长不少于两秒”的断言。
解释为什么只验证第一个不足以证明事件录像正确。
