# 2026-09-27 YOLO 多余检测框修复

## 范围

只修复检测后处理与相应文件索引版本。容器、线程调度、模型文件、640×640 三通道预处理、用户置信度阈值及原 500ms 检测/文件采样策略不变；不采用每三帧检测方案，不修改回放界面。

## 复现与原因

从 DetectionWorker 提取 CPU/GPU 共用的 postprocess 入口，先保持原算法运行确定性张量测试，复现三项失败：

1. 同一人的两个重叠候选与另一人共输出三框；缺少 NMS，期望两框实际三框。
2. 左边界之外的候选仅裁剪左坐标而保留完整宽度，框被扩大。
3. 完全在画面外的候选虽然不产生有效框，仍令 found=true，可能误触发事件。

初次提取接口时修正了 Mat const 类型编译错误；之后测试正常执行，3 项失败构成修复前复现证据，而非用编译失败充当缺陷复现。

## 修改

- 检查原始 YOLOv8/v5 COCO 80 类输出的 FP32、batch=1 和属性轴，支持 [1,N,84/85] 与 [1,84/85,N]；不支持/含歧义的格式返回错误，不伪装为无人帧。
- 保留人类别独立分数策略；v5 使用 objectness×person score，不改变类别筛选策略。
- 过滤非有限值、无效分数、非正尺寸及完全在画面之外的框；按左右/上下端点限幅后计算正确宽高。
- 对有效人框执行 OpenCV NMS，IoU=0.45，优先保留高置信度框；found 和最高置信度仅由最终保留框决定。恰好等于用户置信度阈值的框保留。
- 文件分析 profile 升级到 stretch640-person-nms045-v2，首次使用新版重新分析原视频；旧历史索引、截图及原视频保留。新分析完成后仍可正常复用。

## 已完成的定向验证

- detection_postprocess_tests：13 项通过，覆盖重叠去重、另一人保留、两种输出布局、v5 objectness、阈值边界、坐标缩放/裁剪及无效张量。
- detection_backend_tests：5 项通过，实际 YOLO、CPU/DirectML 结果对照、GPU 缺失后的 CPU 回退与严格模式错误均通过。
- 实际测试模型 models/yolov8n.onnx，视频 artifacts/test-media/people-2s.avi，来自现有 OpenCV vtest 公开测试素材。0/500/1000ms 画面阈值以上的人候选为 27/28/18 个，NMS 后各保留 3 个框；不代表所有用户视频的精确率或召回率已验收。
- source_index_tests：6 项通过，含临时 SQLite 中预置旧版已完成样本后的重新分析、旧结果保留，以及新版本重启复用；不读取或迁移生产索引/数据库。

以上 QtTest 统计包含初始化与清理。全量验证在文档固定后执行，以 artifacts/verification/latest.json 的状态、results 和与当前源码一致的 sourceFingerprint 为准；定向报告副本为 artifacts/verification/nms-focused.json。

## 复验

在工程 PowerShell 中运行：

```powershell
$env:SMART_MONITOR_TEST_MODEL='C:/QtProjects/day20_smart_monitor/models/yolov8n.onnx'
$env:SMART_MONITOR_TEST_VIDEO='C:/QtProjects/day20_smart_monitor/artifacts/test-media/people-2s.avi'
$env:SMART_MONITOR_DETECTION_BACKEND='cpu'
./scripts/build-and-test.ps1
```

后端对照测试会临时切换 DirectML 并恢复环境，其余链路以 CPU 为基线。测试使用隔离数据库和临时媒体。主程序构建于 .workflow/build/day15_pro/bin，正常开发使用 scripts/run-dev.ps1，勿误启动旧 bin 中的历史程序。

## 验收边界

修复的是重复候选、边界裁剪和无效框判定；不承诺消除模型的所有背景误检。严重遮挡下 NMS 的取舍、真实摄像头质量和长稳仍需实测。未修改已有缓存复用时长，因此快速运动导致的旧框位置滞后不属于本次已解决项。