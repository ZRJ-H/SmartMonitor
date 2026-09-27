# 开发与发布工作流

唯一开发目录：C:/QtProjects/day20_smart_monitor，分支 codex/project-completion。
其他两个学习目录保留为历史来源，不再同时修改代码。

## 日常开发

在本目录打开 PowerShell 7：

    # 每次修改：先复现问题，再修改，再运行相关测试
    ./scripts/build-and-test.ps1 -Projects tests/config_compat_tests.pro
    # 合入检查：全部测试和主程序
    ./scripts/build-and-test.ps1
    # 开发启动；第一次排查建议关闭自动采集
    ./scripts/run-dev.ps1 -NoCapture
    # 完整验证后生成独立运行目录
    ./scripts/package-release.ps1

脚本固定 Qt 6.11.2 / MinGW 13.1 / OpenCV 4.14，可通过参数指定本机路径。
源码按 Git 未忽略输入镜像到 .workflow/source，构建产物在 .workflow/build。
不要修改镜像或旧 bin 中的可执行文件。验证报告在 artifacts/verification/latest.json，
每次执行的日志归档在 runs 子目录。失败会返回非零退出码并记录失败状态。
默认自动测试使用临时文件和 SQLite 内存库，不连接实际业务库。

## 安装与数据库

发布包生成于 artifacts/releases，含 Qt/OpenCV/MinGW/QMYSQL 运行依赖、SQL 和维护脚本。
启动前安装兼容的 MySQL 5.7 服务和 Microsoft Visual C++ x64 运行库。全新环境可人工执行 database/smart_monitor_schema.sql；
该文件明确使用 smart_monitor_db，不能不经修改用于别名隔离库。
已有环境仅运行 database/project_completion_migration.sql。迁移前备份，不执行清库。

连接覆盖项：SMART_MONITOR_DB_HOST、SMART_MONITOR_DB_NAME、
SMART_MONITOR_DB_USER、SMART_MONITOR_DB_PASSWORD。
由使用者在当前进程环境中提供密码；不写入命令行参数、仓库、报告。
现有代码的本机兼容默认值尚保留，正式交付应改为专用数据库账户。

    ./scripts/database-maintenance.ps1 -Action Backup
    # 仅在批准具体数据库迁移后执行
    ./scripts/database-maintenance.ps1 -Action Migrate
    # 恢复必须明确目标，优先恢复到新建的隔离库
    ./scripts/database-maintenance.ps1 -Action Restore -Database smart_monitor_verify_example -InputFile '备份.sql' -AllowRestore

维护脚本从 SMART_MONITOR_DB_PASSWORD 读取密码。需要正确的 MySQL 工具路径可传 -MySQLBin。
备份生成 SQL 及 SHA256；恢复前再次备份目标。
应用设置页提供“立即备份数据库”，自动采集模式下启动后检查每日备份，并每小时复查。
应用备份位于 %LOCALAPPDATA%/QtProjects/SmartMonitor/backups，失败不更新成功日期。
SMART_MONITOR_MYSQLDUMP 可覆盖 mysqldump.exe 路径。每日备份仅在应用运行期间执行。

SQL 备份含配置和媒体索引，不含视频、图片、ONNX 文件本体。保留原媒体目录，
将媒体和所选模型另外复制到备份介质；不要先清理原文件再验证恢复。
发布包不伪造或替用户选择 ONNX 模型，在设置中选择实际验证过的模型。

## 使用顺序

1. 设置数据库并完成迁移后启动程序。界面先进入登录页，后台按已保存的通道配置自动采集和录像；启用检测时所有通道持续分析。登录仅解锁查看和操作权限，退出登录只清空画面，不停止后台任务。
2. 配置四路名称和摄像头编号/视频文件、录像和图片目录；保存后相关任务重启。
3. 单/四画面仅控制显示；后台人形检测独立于布局、当前选中通道和登录状态。手动“停止通道”会停止该路采集、录像与检测，必须登录。
4. 事件开始封口普通段，单独录约 60 秒，采集前三个约一秒间隔的真实帧；提前停止记为中断。
5. 图片页按时间、通道、来源查找，双击放大、拖动、前后切换；关联录像可定位回放。
6. 日志页查询操作及事件、打开关联图片/视频。导出和删除会记录审计。

当前是单进程桌面应用，后台任务随进程存在；关闭整个程序会停止并封口录像，尚未安装独立 Windows 服务。开发参数 `-NoCapture` / `SMART_MONITOR_AUTOSTART=0` 仍用于关闭自动采集及自动维护。

相对录像/图片目录以程序所在目录为基准；长期使用建议保存绝对目录。
媒体已封口但 SQL 失败时，清单和事件帧保留在
%LOCALAPPDATA%/QtProjects/SmartMonitor/pending-media，启动和每分钟重试。
不要删除待恢复清单。突然断电导致尚未封口的 MP4 损坏不在已封口恢复保证内。

## 验收与结项

当前属于集成候选版。本机生产库已于 2026-09-23 获授权迁移；实际摄像头/模型端到端演示、干净 Windows 安装、
7×24 小时长稳必须分别保留证据后才能结项。
观察现有应用进程：

    ./scripts/observe-runtime.ps1 -ProcessId <程序PID> -Minutes 10080

该脚本记录内存、CPU 累计时间、句柄及响应状态，不代替帧率、媒体数量和人工演示验收。
修改运行代码或中途崩溃后重新确定长稳观察起点。

## 可执行目录依赖

构建主程序时自动运行 deploy-runtime.ps1，并使用仅系统 PATH 的子进程验证启动、Windows 平台插件及 SQLite/QMYSQL 插件加载。修复现有 bin 目录可执行：

    ./scripts/deploy-runtime.ps1 -OutputDir ./bin -CleanStart

双击 bin/day20_smart_monitor.exe 时需保留整个 bin 目录（DLL、platforms、sqldrivers、multimedia、qt.conf），不要只移动 EXE。发布请使用 package-release.ps1 生成的完整目录。


## 评审整改工作流

运行 ./scripts/verify-review.ps1 执行包含真实 YOLO 的完整回归和界面截图。运行模式可用 ./scripts/run-dev.ps1 -Pipeline preview、record、full 分离负载；每次先退出旧进程。详见 [评审整改记录](verification/review-remediation-2026-09-24.md)。

## 2026-09-25 GPU 与文件事件变更

本地文件不再重复编码录像；按内容和模型版本分析一次，每个去重事件保存一张代表截图，事件数量不限。回放定位原视频。摄像头仍走原录像链路。GPU 使用可选 ONNX Runtime DirectML，默认 auto，失败明确回退 CPU。部署、独立 SQLite 索引备份与验收边界见 [实施验证](verification/gpu-recording-2026-09-25.md) 和 [运行环境](runtime-setup.md)。

## MainWidget 组件导航（2026-09-27）

MainWidget 只负责导航、会话和模块连接。独立页面为 view/playbackpage、monitorpage、settingspage；后台协调和维护位于 service/monitoringcontroller、maintenancecontroller。状态归属、信号流、分阶段验证与故障定位见 [组件拆分与审查记录](verification/mainwidget-components-2026-09-27.md)。

主程序与两个集成测试统一 include view/mainwidget.pri。新增组件需更新此构建清单；不要向 MainWidget 暴露页面控件，不向控制器传入页面或 MainWidget 指针。
