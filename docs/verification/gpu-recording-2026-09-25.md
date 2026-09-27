# GPU 检测与文件事件索引：实施及验证

日期：2026-09-25。对应方案：../gpu-and-recording-plan-2026-09-25.md。

## 已实施

- 实际 GPU 路线采用 ONNX Runtime 1.22.0 + DirectML 1.15.4，Qt/MinGW 工具链保持不变。当前 OpenCV 没有 CUDA 后端；DirectML 已在 RTX 4060 Laptop GPU 上完成真实 YOLO 推理，独立 profiling 记录 DmlExecutionProvider。没有将此结果描述为 CUDA 推理。
- 进程内按模型版本共享会话，使用 FIFO ticket 限制一个在途执行；每路采集不等待推理，文件分析与实时摄像头共享后端。auto 模式失败明确降级 CPU；directml 严格模式失败返回错误。
- 本地文件只做一次源时间分析，连续预览不创建普通/事件录像。内容 SHA-256、模型与采样配置决定分析身份，同内容多通道共享分析和截图。
- 每 500 ms 源时间取样，阳性间隔不超过 3 秒合并；EOF 封口；每个去重事件仅保存第一张真实代表截图，事件数量不限。此采样策略不保证捕获短于采样间隔的目标。
- 使用独立应用数据目录中的 SQLite 索引（schema version 1），持久化样本、检查点、事件、当前及历史通道关系。无需修改现有业务 MySQL 结构；摄像头旧表及历史录像保留。
- 回放页提供“文件事件”标签、分页、通道筛选、历史版本、截图查看和原视频重定位。回放前后台验证内容，拒绝同名替换文件；源位置与分析时间分开显示。
- 摄像头媒体由记录线程先写原子恢复清单，再交给有界存储队列入库；8 个排队任务上限，满时只反压记录线程。数据库连接在存储线程创建。图片编码、事件入库、重试及媒体审计不在 GUI 执行。正常保存复用原图，不再立即解码刚编码的恢复图片。
- GPU 运行库与依赖固定哈希下载到忽略目录，部署至 EXE 旁 gpu/，携带许可证。新增 --gpu-check <model> 严格诊断；--runtime-check 在干净环境验证驱动插件，不进入业务。
- 所有测试使用临时 SQLite 和临时媒体；没有读取、迁移或修改生产数据库，没有删除原视频或历史媒体。

## 可核对的证据

原始公开素材来自 [OpenCV 官方 vtest.avi](https://github.com/opencv/opencv/blob/4.12.0/samples/data/vtest.avi)，仅作本地回归测试，不随发布包分发。
SHA-256：45cddc9490be69345cbdab64ca583be65987e864ca408038e648db99e10516cf。
原片 768×576、10 FPS、795 帧；短片由前 20 个真实帧编码为 2 秒 AVI。

- artifacts/verification/gpu-recording/probe.txt：同一真实帧预热后 20 次，CPU 推理 P50/P95 约 73.31/82.57 ms，GPU 约 7.30/7.86 ms；原始输出最大绝对差约 0.001831。仅代表该固定输入基准。
- artifacts/verification/gpu-recording/directml_*.json：ORT profiling，节点执行提供者为 DmlExecutionProvider。
- artifacts/verification/gpu-recording/baseline-test.txt：修改前同一真实视频四路运行 65 秒，GUI 心跳最大 1532 ms，生成 12 个 MP4、12 张 JPG，临时媒体约 46.99 MB。
- artifacts/verification/gpu_file_workflow_tests-test.txt：修改后真实文件分析、原视频播放、原文件 hash 不变、退出重启复用、零 MP4，以及心跳/存储量；由每次运行刷新。
- 两秒片段的 45 秒定向运行覆盖约 22 次实际循环，零 MP4、一张事件截图；重启后推理次数为零、截图数量不增加。
- artifacts/verification/source_index_tests-test.txt：真实人物/空画面片段验证两个事件、四通道共享、检查点恢复、移动/替换、无效模型。
- artifacts/verification/storage_worker_tests-test.txt：持久化后数据库失败重放一次、慢数据库锁不阻塞事件循环。
- artifacts/verification/detection_backend_tests-test.txt：GPU/CPU 人物判定、置信度与框数量对照；缺失 GPU 运行库时 auto 明确降级、strict 失败。

完整构建与测试以 artifacts/verification/latest.json 的 passed 状态、覆盖工程数和当前源码指纹一致为准，不用上述定向结果代替完整回归。较长运行的结果单独写入 artifacts/verification/gpu-recording/endurance.txt 与 endurance-resources.csv，未生成有效报告前不宣称长时验证通过。

## 运行与复现

```powershell
./scripts/setup-gpu-runtime.ps1
./scripts/download-test-video.ps1
$env:SMART_MONITOR_TEST_MODEL='C:/QtProjects/day20_smart_monitor/models/yolov8n.onnx'
$env:SMART_MONITOR_TEST_VIDEO='C:/QtProjects/day20_smart_monitor/artifacts/test-media/vtest.avi'
$env:SMART_MONITOR_DETECTION_BACKEND='directml'
./scripts/build-and-test.ps1
```

正式程序默认 auto，无需设置上述测试环境变量。强制 CPU 排查可设置 SMART_MONITOR_DETECTION_BACKEND=cpu；GPU 依赖检查使用程序 --gpu-check 后跟模型路径。运行库无需修改系统 PATH。

## 保留边界

- 文件分析默认 0.5 秒抽样，不宣称逐帧检测。摄像头仍沿用原连续/约 60 秒事件录像规则及最高 10 FPS 编码。
- 独立 SQLite 索引和截图默认在 %LOCALAPPDATA%/QtProjects/SmartMonitor/file-index；可通过 SMART_MONITOR_INDEX_DIR 指定目录。备份此目录时先关闭程序并连同截图整体保存；原视频另行保存。原业务 MySQL 备份不包含此目录。
- 当前保留原帧 latest-frame 的预览和录像邮箱。GPU 推理加速不等于摄像头逐帧无损录像；记录线程被磁盘反压时可能覆盖未消费帧。
- 极端慢/失效存储会显示失败并保留可恢复清单；进程退出仍要等待正在执行的解码/推理/录像封口及存储线程收尾。未宣称解决所有设备驱动无限阻塞。
- Qt offscreen 的回放窗口截图可能不包含硬件合成的视频表面；测试另验证真实 QVideoSink 帧可解码并保存 decoded-original-frame.png，不以空白截图或界面模拟作为媒体成功证据。
- 真实四摄像头、全新 Windows 安装、断电和 7×24 小时长稳仍开放。本次短/较长本机测试不替代这些验收。
