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
