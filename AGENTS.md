# 工作约定

- 本目录是唯一开发工程；不要向历史学习副本同步写入。
- 先复现缺陷，再修改，按影响范围测试；提交前运行 scripts/build-and-test.ps1。
- 保留真实 MonitorTask / RecordTask / QMediaPlayer 链路，不以界面模拟替代媒体实现。
- 自动测试默认只用 SQLite 或显式 smart_monitor_verify_* 隔离库。
- 不删除原视频、图片或备份。生产数据库迁移需要确认具体结构变更，先备份并在隔离库验证。
- 避免把密码、媒体、运行库、构建结果放入 Git。
- 使用 docs/DEVELOPMENT.md 中的显式工具链，不依赖当前 PATH 中的 qmake/make。
- 更改后更新 docs/verification/acceptance.md；未完成的硬件和长稳验收保持开放。
