# 数据与恢复契约

- record.directory、record.segment_seconds、channel.N JSON 是实际工程的优先配置。
  兼容 video_dir、record_segment_seconds、channel_N_name/source，保存时同一事务同步。
  现有优先值不被旧键覆盖；相对存储路径按程序目录解释。
- video.id 是图片回放定位的录像 ID；exception_log.id 是事件 ID，二者不可互换。
  event_media_link.event_key 为唯一业务键，关联 exception_id 和 video_id。
  monitor_event 保留旧查询兼容用途，运行时不自动建表。
- 图片 video_offset_ms 表示对应录像内的位置，预览手动截图没有已封口录像时不编造录像 ID。
- 事件元数据用事务提交；图片三张成组入库。缺帧不能记 complete，提前结束记 interrupted。
  入库/图片失败保留恢复清单并重试；重复 complete 事件不重复创建媒体。
- 恢复清单先用 QSaveFile 原子落盘，再执行 SQL，成功后才删除清单；
  文件不存在、清单损坏或 SQL 错误保留待办并显示原因。
- operation_log.admin_id=NULL 表示系统任务，不伪造管理员。主业务成功但审计失败单独提示。
- 数据库迁移 SQL 针对现有基础表，不依赖隐式运行查询创建结构。
  本次实际旧库 monitor_event 为 0 行；存在历史非空旧事件的其他安装需先评估 ID 回填。


## 本地文件索引（2026-09-25）

- 文件源不调用 RecordTask 重录。外部原视频内容 SHA-256 为资产标识，模型 hash/阈值/采样与合并规则为分析版本；SQLite 唯一键保证重复提交幂等。
- 文件索引在应用 file-index/source-index.sqlite（user_version=1），与既有 MySQL 分离；assets、analyses、channel_analysis、analysis_channels、events、samples 只由源分析线程访问。不存在隐式生产 MySQL 迁移。
- 每个文件事件一张代表 JPG，按源 PTS 保存起止位置；不把源位置叠加到分析时间伪造拍摄日期。相同文件在多通道使用不复制截图，历史分析可查询。
- 外部源没有自动删除路径；回放前验证内容，移动可重新定位，相同路径不同内容不能复用旧事件回放。
- 摄像头旧媒体契约保留；RecordTask 线程先 stage 原子清单，再提交 StorageWorker 独立连接入库。关闭后未入库清单仍可重放。正常保存复用已有帧而非马上解码恢复 PNG。
