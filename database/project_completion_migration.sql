-- Compatible additions only; run after smart_monitor_schema.sql on a fresh database.
CREATE TABLE IF NOT EXISTS monitor_event (
 id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
 event_key VARCHAR(80) NOT NULL UNIQUE,
 channel_id INT NOT NULL,
 event_time DATETIME NOT NULL,
 video_path VARCHAR(1024),
 confidence DOUBLE NOT NULL,
 create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS event_media_link (
 event_key VARCHAR(80) NOT NULL PRIMARY KEY,
 exception_id INT NOT NULL,
 video_id INT NOT NULL,
 CONSTRAINT fk_event_link_exception FOREIGN KEY(exception_id) REFERENCES exception_log(id) ON DELETE CASCADE,
 CONSTRAINT fk_event_link_video FOREIGN KEY(video_id) REFERENCES video(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
-- A system warning is not an administrator action. NULL preserves that distinction.
ALTER TABLE operation_log MODIFY admin_id INT NULL;
-- Preserve the live project's established values when compatibility keys also exist.
INSERT INTO sys_config(config_key,config_value)
SELECT 'record.directory',config_value FROM (SELECT config_value FROM sys_config WHERE config_key='video_dir') s
ON DUPLICATE KEY UPDATE config_key=VALUES(config_key);
INSERT INTO sys_config(config_key,config_value)
SELECT 'record.segment_seconds',config_value FROM (SELECT config_value FROM sys_config WHERE config_key='record_segment_seconds') s
ON DUPLICATE KEY UPDATE config_key=VALUES(config_key);

-- Event-type filtering uses a correlated video_path lookup; keep it indexed on old installations too.
SET @event_path_index_sql = IF(
 (SELECT COUNT(*) FROM information_schema.statistics WHERE table_schema=DATABASE()
   AND table_name='monitor_event' AND index_name='idx_monitor_event_video_path') = 0,
 'ALTER TABLE monitor_event ADD INDEX idx_monitor_event_video_path(video_path(191))',
 'SELECT 1');
PREPARE event_path_index_stmt FROM @event_path_index_sql;
EXECUTE event_path_index_stmt;
DEALLOCATE PREPARE event_path_index_stmt;
