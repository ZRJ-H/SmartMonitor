# 开发与发布工作流

唯一开发目录：C:/QtProjects/day20_smart_monitor，分支 codex/project-completion。
其他两个学习目录保留为历史来源，不再同时修改代码。

## 日常开发

在本目录打开 PowerShell：

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

1. 设置数据库并完成迁移后启动程序。访客可看实时画面，登录后管理配置、回放、图片与日志。
2. 配置四路名称和摄像头编号/视频文件、录像和图片目录；保存后相关任务重启。
3. 四画面运行普通录像；切单画面后只对所选通道运行启用的人形检测。
4. 事件开始封口普通段，单独录约 60 秒，采集前三个约一秒间隔的真实帧；提前停止记为中断。
5. 图片页按时间、通道、来源查找，双击放大、拖动、前后切换；关联录像可定位回放。
6. 日志页查询操作及事件、打开关联图片/视频。导出和删除会记录审计。

相对录像/图片目录以程序所在目录为基准；长期使用建议保存绝对目录。
媒体已封口但 SQL 失败时，清单和事件帧保留在
%LOCALAPPDATA%/QtProjects/SmartMonitor/pending-media，启动和每分钟重试。
不要删除待恢复清单。突然断电导致尚未封口的 MP4 损坏不在已封口恢复保证内。

## 验收与结项

当前属于集成候选版。生产库迁移、实际摄像头/模型端到端演示、干净 Windows 安装、
7×24 小时长稳必须分别保留证据后才能结项。
观察现有应用进程：

    ./scripts/observe-runtime.ps1 -ProcessId <程序PID> -Minutes 10080

该脚本记录内存、CPU 累计时间、句柄及响应状态，不代替帧率、媒体数量和人工演示验收。
修改运行代码或中途崩溃后重新确定长稳观察起点。
