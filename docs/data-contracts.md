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
