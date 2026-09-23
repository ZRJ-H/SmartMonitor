-- REQ-08 / REQ-09 / REQ-10: Day 7-8 image, settings and log storage.
-- Safe to execute repeatedly on MySQL 5.7.
SET NAMES utf8mb4;
USE smart_monitor_db;

CREATE TABLE IF NOT EXISTS feature_image (
  id INT NOT NULL AUTO_INCREMENT,
  image_name VARCHAR(128) NOT NULL,
  image_path VARCHAR(255) NOT NULL,
  channel_id INT NOT NULL,
  capture_time DATETIME NOT NULL,
  capture_source VARCHAR(16) NOT NULL DEFAULT 'manual',
  exception_id INT NULL,
  video_id INT NULL,
  video_offset_ms BIGINT UNSIGNED NULL,
  create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_feature_image_path (image_path),
  KEY idx_image_channel_capture (channel_id,capture_time),
  KEY idx_image_exception (exception_id),
  KEY idx_image_video (video_id),
  CONSTRAINT fk_feature_image_channel FOREIGN KEY (channel_id)
    REFERENCES camera_channel(id),
  CONSTRAINT fk_feature_image_exception FOREIGN KEY (exception_id)
    REFERENCES exception_log(id) ON DELETE SET NULL,
  CONSTRAINT fk_feature_image_video FOREIGN KEY (video_id)
    REFERENCES video(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS sys_config (
  id INT NOT NULL AUTO_INCREMENT,
  config_key VARCHAR(64) NOT NULL,
  config_value VARCHAR(255) NULL,
  update_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_sys_config_key (config_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS operation_log (
  id INT NOT NULL AUTO_INCREMENT,
  admin_id INT NOT NULL,
  operation_time DATETIME NOT NULL,
  operation_func VARCHAR(64) NOT NULL,
  operation_desc VARCHAR(255) NULL,
  PRIMARY KEY (id),
  KEY idx_operation_time_id (operation_time,id),
  KEY idx_operation_admin_time (admin_id,operation_time),
  KEY idx_operation_func_time (operation_func,operation_time),
  CONSTRAINT fk_operation_log_admin FOREIGN KEY (admin_id)
    REFERENCES admin_user(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO sys_config(config_key,config_value) VALUES
  ('video_dir','./records'),
  ('image_dir','./images'),
  ('record_segment_seconds','60'),
  ('capture_interval_ms','40'),
  ('channel_1_name','通道1'),
  ('channel_1_source','camera:0'),
  ('channel_2_name','通道2'),
  ('channel_2_source',''),
  ('channel_3_name','通道3'),
  ('channel_3_source',''),
  ('channel_4_name','通道4'),
  ('channel_4_source','')
ON DUPLICATE KEY UPDATE config_key=VALUES(config_key);
