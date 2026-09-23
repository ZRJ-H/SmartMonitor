# 集成候选版验收记录（2026-09-23）

## 工程状态

唯一工程：C:/QtProjects/day20_smart_monitor；分支：codex/project-completion。
基线 dc676ea，集成检查点 58f255d；最终修复提交见 Git 日志。
历史来源 local-day78/completion 对应 564d9e4，原学习目录保留。

已实现：真实采集/录像链路保留；图片九宫格及查看器；真实帧截图；
关联录像定位；普通/事件筛选；日志与事件查询；兼容配置及事务；
事件录像和三图；已封口媒体恢复；审计身份；手动/每日备份；
构建、开发启动、发布打包和指标采集脚本。

## 自动验证

17 个默认测试工程，83 个 QtTest 通过项，0 失败、0 跳过。
通过项包含 init/cleanup，不等于独立业务用例数。
最终状态以 artifacts/verification/latest.json 为准；报告记录源码指纹、
构建提交、工作树状态和 EXE SHA256。打包必须匹配指纹及二进制。
定向回归覆盖稀疏帧实际解码时长、崩溃遗留 JPG 安全收养、损坏清单后的恢复公平性。

额外 tests/mysql_integration.pro 只允许 smart_monitor_verify_ 前缀数据库，
实际 QMYSQL 驱动及外键、中文图片路径、重复事件、MP4 解码、异步 mysqldump 备份通过。
3 个 QtTest 通过项（1 个实际业务用例，另 2 个初始化/清理）。

## 迁移和恢复

实际库原有录像 198、配置 10、图片 0、旧 monitor_event 0。
只读生产备份：artifacts/backups/smart_monitor_db-before-completion-20260923_105750.sql。
恢复至 smart_monitor_verify_20260923_105750，两次迁移后原记录数不变。
之后仅隔离库增加测试事件及三图，验证真实外键和文件可打开。

用户于 2026-09-23 明确批准后，生产迁移已完成。
迁移前备份：artifacts/backups/smart_monitor_db-20260923-114412-788.sql，
SHA256：47bf2116e9bfe65b9b240a4ba15c9fd1098830ee315af638e68b442af46e2fa8。
event_media_link 表及两个外键、日志管理员可空字段、事件路径索引均已核对。
录像 198、配置 10、图片/事件/操作日志各 0、管理员 3、通道 4，迁移前后数量一致；
原配置值不变，本次没有需要补入的配置键。
证据：artifacts/verification/production-migration.json。旧隔离库演练证据另行保留。

第一次调用旧 powershell.exe 时，备份脚本因未加载 Get-FileHash 在结构迁移前停止；
随后用已验证的 PowerShell 7 重新备份、迁移并完成核对，没有绕过失败步骤。

## 性能

隔离库 smart_monitor_verify_perf_20260923：100000 录像、1000 事件。
各查询运行 5 次，包含 mysql 客户端启动和连接耗时。
缺索引时事件计数超过 3000 ms。
补索引后最大耗时：总数 74.41 ms、首页 110.93 ms、通道页 52.21 ms、
事件计数 674.06 ms、事件页 724.05 ms。
完整 SQL、EXPLAIN、系统信息及采样见 artifacts/verification/performance.json。
这是本机 SQL 测试，不替代真实采集负载下的 UI 性能。

## 独立审查及修复

独立审查基于 58f255d，发现 4 项 Important，均已修复并定向验证：
1. 固定 FPS 与墙钟偏移不一致：补帧保持时间轴，用实际写入帧号生成偏移。
2. 图片落盘和 SQL 提交之间崩溃：仅收养编码内容完全相同的事件 JPG，未知文件不覆盖。
3. 前 20 个永久错误导致恢复饥饿：持久化轮转游标，两轮内恢复第 21 项。
4. 旧 EXE 可能冒充当前源码：验证报告与打包绑定源指纹、EXE hash 和实际构建身份。
另修正系统审计归因，手动备份保留发起者 ID。

保留现场验证：阻塞摄像头退出、慢编码队列积压、真实模型精度及冷却效果。
没有把未复现风险当作已修复或已通过。

## 发布与未完成验收

发布目录见 artifacts/releases/latest.txt。
已在仅 Windows 系统 PATH、无外部 Qt/OpenCV 搜索路径的进程中成功启动并连接隔离 MySQL；
最终包复测结果见 artifacts/verification/package-smoke.json。
这是开发机依赖隔离测试，不是干净 Windows 首次安装验收。
MySQL 客户端还依赖 Microsoft Visual C++ x64 运行库，部署机必须安装。

保持开放：
- 真实摄像头和 ONNX 模型的有人/无人/进出画面演示及完整约 60 秒事件验证。
- 干净 Windows 安装、实际 USB 导出、低磁盘和设备拔插测试。
- 7×24 小时连续运行，无卡死、持续积压或明显内存增长。
- 用恢复数据完成现场回放、图片定位和备份媒体核验后确认最终结项。

SQL 备份含结构、配置和索引；媒体文件与模型需独立备份。
突然断电时未封口 MP4 的修复不在当前保证范围。

## 接管与带学

从 [学习入口](../learning/README.md) 开始；学习进度与业务验收分别记录。
