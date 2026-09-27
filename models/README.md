# YOLOv8n 人形检测模型

本机文件：`C:/QtProjects/day20_smart_monitor/models/yolov8n.onnx`（不加入 Git）。

- 官方固定版本来源：https://github.com/ultralytics/assets/releases/download/v8.4.0/yolov8n.onnx
- 下载入口依据：https://github.com/ultralytics/inference
- SHA256：`b2bc52f40e8e1c532427d5bde3575a5d5b571b739fab2c6df443733ed1589cbd`
- 大小：12851049 字节；输入 FP32 `[1,3,640,640]`，输出 `[1,84,8400]`。
- COCO 检测模型，person 是第 0 类；无内置 NMS，后处理由 MonitorTask 执行。
- 使用遵循 Ultralytics 模型许可：https://www.ultralytics.com/license

OpenCV 4.14 样例验证可推理，真实 MonitorTask 已发出 personDetected 信号。
这不等于真实摄像头完整约 60 秒事件、模型精度或长稳验收。
第 19 课的 face/lite 多输出模型不适配本项目当前解析器，不能仅凭能加载就认为可用。

登录系统，在设置页选择上述绝对路径，开启检测、阈值设为 0.50，保存后切到单画面。
四宫格模式保持检测；本地文件完成一次分析后复用结果。数据库配置保存状态以实际设置页为准。
