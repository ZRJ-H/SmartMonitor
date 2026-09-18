-- Day15 视频分页演示数据。
-- 每条通过 video_path 判断是否已存在，可重复执行。

SET NAMES utf8mb4;
USE smart_monitor_db;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '东门演示录像01', 'E:/monitor/video/day15_demo_01.mp4', id,
       '2026-08-25 08:00:00', '2026-08-25 08:10:00', 52428800
FROM camera_channel
WHERE channel_num = 'CAM-EAST-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_01.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '西门演示录像02', 'E:/monitor/video/day15_demo_02.mp4', id,
       '2026-08-25 09:00:00', '2026-08-25 09:12:00', 60817408
FROM camera_channel
WHERE channel_num = 'CAM-WEST-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_02.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '停车场演示录像03', 'E:/monitor/video/day15_demo_03.mp4', id,
       '2026-08-25 10:00:00', '2026-08-25 10:15:00', 73400320
FROM camera_channel
WHERE channel_num = 'CAM-PARK-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_03.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '机房演示录像04', 'E:/monitor/video/day15_demo_04.mp4', id,
       '2026-08-25 11:00:00', '2026-08-25 11:08:00', 44040192
FROM camera_channel
WHERE channel_num = 'CAM-SERVER-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_04.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '东门演示录像05', 'E:/monitor/video/day15_demo_05.mp4', id,
       '2026-08-25 12:00:00', '2026-08-25 12:11:00', 57671680
FROM camera_channel
WHERE channel_num = 'CAM-EAST-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_05.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '西门演示录像06', 'E:/monitor/video/day15_demo_06.mp4', id,
       '2026-08-25 13:00:00', '2026-08-25 13:14:00', 68157440
FROM camera_channel
WHERE channel_num = 'CAM-WEST-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_06.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '停车场演示录像07', 'E:/monitor/video/day15_demo_07.mp4', id,
       '2026-08-25 14:00:00', '2026-08-25 14:09:00', 49283072
FROM camera_channel
WHERE channel_num = 'CAM-PARK-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_07.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '机房演示录像08', 'E:/monitor/video/day15_demo_08.mp4', id,
       '2026-08-25 15:00:00', '2026-08-25 15:13:00', 62914560
FROM camera_channel
WHERE channel_num = 'CAM-SERVER-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_08.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '东门演示录像09', 'E:/monitor/video/day15_demo_09.mp4', id,
       '2026-08-26 08:00:00', '2026-08-26 08:10:00', 54525952
FROM camera_channel
WHERE channel_num = 'CAM-EAST-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_09.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '西门演示录像10', 'E:/monitor/video/day15_demo_10.mp4', id,
       '2026-08-26 09:00:00', '2026-08-26 09:12:00', 59768832
FROM camera_channel
WHERE channel_num = 'CAM-WEST-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_10.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '停车场演示录像11', 'E:/monitor/video/day15_demo_11.mp4', id,
       '2026-08-26 10:00:00', '2026-08-26 10:15:00', 72351744
FROM camera_channel
WHERE channel_num = 'CAM-PARK-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_11.mp4')
LIMIT 1;

INSERT INTO video (video_name, video_path, channel_id, start_time, end_time, file_size)
SELECT '机房演示录像12', 'E:/monitor/video/day15_demo_12.mp4', id,
       '2026-08-26 11:00:00', '2026-08-26 11:08:00', 42991616
FROM camera_channel
WHERE channel_num = 'CAM-SERVER-01'
  AND NOT EXISTS (SELECT 1 FROM video WHERE video_path = 'E:/monitor/video/day15_demo_12.mp4')
LIMIT 1;

SELECT COUNT(*) AS day15_demo_video_count
FROM video
WHERE video_path LIKE 'E:/monitor/video/day15_demo_%';
