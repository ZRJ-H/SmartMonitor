# 录像编码与 YOLO 模型配置

## OpenH264

`Failed to load OpenH264 library: openh264-2.5.0-win64.dll` 表示缺少 H.264 编码运行库。
RecordTask 每段先尝试 H.264，再回退 mp4v；不能单凭该报错断言全部录像失败。

本机已独立下载并安装到 `C:/QtProjects/day20_smart_monitor/bin`。
官方发布页：https://github.com/cisco/openh264/releases/tag/v2.5.0
官方二进制：http://ciscobinary.openh264.org/openh264-2.5.0-win64.dll.bz2
解压后文件名必须保留 `openh264-2.5.0-win64.dll`，放到实际 EXE 同目录，然后重启程序。
本次文件有效数字签名为 Cisco WebEx LLC，SHA256：
`bf91c488638b32448f7d89611f6bb9c232dce82975d7d2827c78f78fc2a7fb2b`。

完整 Cisco 二进制许可保留在 `docs/third-party/openh264-binary-license.txt`。
发布包不捆绑该 Cisco 二进制，目标机器需从官方单独下载安装；本机开发构建可复用已安装的文件。

运行 `./tests/verify_runtime_dependencies.ps1 -OutputDir ./bin -RequireH264` 检查文件是否齐全。
实际编码能力仍需写入、封口、解码验证。本次探针实际生成 H.264 并回读 20 帧。
构建脚本把依赖复制到真实 EXE 输出目录；Qt Creator 默认输出 `bin`，工作流输出 `.workflow/build/day15_pro/bin`。

## YOLO

已安装的兼容模型：`C:/QtProjects/day20_smart_monitor/models/yolov8n.onnx`。
官方来源、哈希和输入输出见工程 `models/README.md`。
其他电脑可从固定官方来源下载模型，按实际安装位置在设置页选择。

1. 登录程序，打开设置页。
2. 选择 YOLO ONNX 模型的完整绝对路径，开启智能检测，置信度先用 `0.50`。
3. 保存检测设置后，所有已配置的通道在后台执行检测；单/四画面切换和退出登录不影响检测。
4. 使用有人画面确认绿色框、事件记录和后续关联媒体；四宫格暂停 YOLO 检测。

旧课程的 face/lite 模型是不同的输出格式，不可替代该标准 COCO 模型。
仅出现“智能检测已启用”不等于模型格式已通过验证；本次模型已通过真实 MonitorTask 的人形事件信号验证。
Qt 的 `Using Qt multimedia with FFmpeg ...` 与“数据库连接成功”是正常启动信息。
