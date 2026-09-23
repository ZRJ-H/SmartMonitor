CREATE DATABASE IF NOT EXISTS smart_monitor_db
  CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE smart_monitor_db;

CREATE TABLE IF NOT EXISTS admin_user (
  id INT NOT NULL AUTO_INCREMENT,
  username VARCHAR(32) NOT NULL,
  password VARCHAR(128) NOT NULL,
  create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_login_time DATETIME NULL,
  status TINYINT NOT NULL DEFAULT 1,
  PRIMARY KEY (id),
  UNIQUE KEY username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS camera_channel (
  id INT NOT NULL AUTO_INCREMENT,
  channel_num VARCHAR(32) NOT NULL,
  camera_name VARCHAR(64) NOT NULL,
  is_online TINYINT NOT NULL DEFAULT 0,
  create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  update_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY channel_num (channel_num)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS exception_log (
  id INT NOT NULL AUTO_INCREMENT,
  channel_id INT NOT NULL,
  event_type VARCHAR(32) NOT NULL DEFAULT 'person',
  event_time DATETIME NOT NULL,
  event_end_time DATETIME NULL,
  confidence DECIMAL(5,4) NULL,
  event_status VARCHAR(16) NOT NULL DEFAULT 'new',
  event_desc VARCHAR(255) NULL,
  related_video_path VARCHAR(255) NULL,
  create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY fk_exception_log_channel (channel_id),
  KEY idx_exception_channel_time (channel_id,event_time),
  KEY idx_exception_type_time (event_type,event_time),
  KEY idx_exception_status_time (event_status,event_time),
  CONSTRAINT fk_exception_log_channel FOREIGN KEY (channel_id)
    REFERENCES camera_channel(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS video (
  id INT NOT NULL AUTO_INCREMENT,
  video_name VARCHAR(128) NOT NULL,
  video_path VARCHAR(255) NOT NULL,
  channel_id INT NOT NULL,
  start_time DATETIME NOT NULL,
  end_time DATETIME NULL,
  file_size BIGINT UNSIGNED NULL,
  video_type VARCHAR(16) NOT NULL DEFAULT 'normal',
  exception_id INT NULL,
  create_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_video_path (video_path),
  KEY fk_video_channel (channel_id),
  KEY fk_video_exception (exception_id),
  KEY idx_video_channel_start (channel_id,start_time),
  KEY idx_video_type_start (video_type,start_time),
  CONSTRAINT fk_video_channel FOREIGN KEY (channel_id) REFERENCES camera_channel(id),
  CONSTRAINT fk_video_exception FOREIGN KEY (exception_id)
    REFERENCES exception_log(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

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
  KEY fk_feature_image_channel (channel_id),
  KEY fk_feature_image_exception (exception_id),
  KEY idx_image_channel_capture (channel_id,capture_time),
  KEY idx_image_video (video_id),
  CONSTRAINT fk_feature_image_channel FOREIGN KEY (channel_id) REFERENCES camera_channel(id),
  CONSTRAINT fk_feature_image_exception FOREIGN KEY (exception_id)
    REFERENCES exception_log(id) ON DELETE SET NULL,
  CONSTRAINT fk_feature_image_video FOREIGN KEY (video_id)
    REFERENCES video(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS operation_log (
  id INT NOT NULL AUTO_INCREMENT,
  admin_id INT NOT NULL,
  operation_time DATETIME NOT NULL,
  operation_func VARCHAR(64) NOT NULL,
  operation_desc VARCHAR(255) NULL,
  PRIMARY KEY (id),
  KEY fk_operation_log_admin (admin_id),
  KEY idx_operation_admin_time (admin_id,operation_time),
  KEY idx_operation_func_time (operation_func,operation_time),
  CONSTRAINT fk_operation_log_admin FOREIGN KEY (admin_id) REFERENCES admin_user(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS sys_config (
  id INT NOT NULL AUTO_INCREMENT,
  config_key VARCHAR(64) NOT NULL,
  config_value VARCHAR(255) NULL,
  update_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY config_key (config_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

DROP PROCEDURE IF EXISTS day1_add_column_if_missing;
DROP PROCEDURE IF EXISTS day1_migrate_config_key;
DROP PROCEDURE IF EXISTS day1_add_index_if_missing;
DROP PROCEDURE IF EXISTS day1_ensure_nullable_fk;

DELIMITER $$

CREATE PROCEDURE day1_add_column_if_missing(
    IN p_table_name VARCHAR(64),
    IN p_column_name VARCHAR(64),
    IN p_definition VARCHAR(1000)
)
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = p_table_name
          AND COLUMN_NAME = p_column_name
    ) THEN
        SET @day1_sql = CONCAT(
            'ALTER TABLE `', REPLACE(p_table_name, '`', '``'),
            '` ADD COLUMN `', REPLACE(p_column_name, '`', '``'), '` ',
            p_definition
        );
        PREPARE day1_stmt FROM @day1_sql;
        EXECUTE day1_stmt;
        DEALLOCATE PREPARE day1_stmt;
    END IF;
END$$

CREATE PROCEDURE day1_migrate_config_key()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'sys_config' AND COLUMN_NAME = 'config_keyy'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'sys_config' AND COLUMN_NAME = 'config_key'
    ) THEN
        ALTER TABLE sys_config CHANGE COLUMN config_keyy config_key VARCHAR(64) NOT NULL;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'sys_config' AND INDEX_NAME = 'config_keyy'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'sys_config' AND INDEX_NAME = 'config_key'
    ) THEN
        ALTER TABLE sys_config RENAME INDEX config_keyy TO config_key;
    END IF;
END$$

CREATE PROCEDURE day1_add_index_if_missing(
    IN p_table_name VARCHAR(64),
    IN p_index_name VARCHAR(64),
    IN p_index_ddl VARCHAR(1000)
)
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = p_table_name
          AND INDEX_NAME = p_index_name
    ) THEN
        SET @day1_sql = p_index_ddl;
        PREPARE day1_stmt FROM @day1_sql;
        EXECUTE day1_stmt;
        DEALLOCATE PREPARE day1_stmt;
    END IF;
END$$

CREATE PROCEDURE day1_ensure_nullable_fk(
    IN p_table_name VARCHAR(64),
    IN p_constraint_name VARCHAR(64),
    IN p_drop_ddl VARCHAR(1000),
    IN p_add_ddl VARCHAR(1000)
)
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.REFERENTIAL_CONSTRAINTS
        WHERE CONSTRAINT_SCHEMA = DATABASE()
          AND TABLE_NAME = p_table_name
          AND CONSTRAINT_NAME = p_constraint_name
          AND DELETE_RULE = 'SET NULL'
    ) THEN
        IF EXISTS (
            SELECT 1 FROM information_schema.TABLE_CONSTRAINTS
            WHERE CONSTRAINT_SCHEMA = DATABASE()
              AND TABLE_NAME = p_table_name
              AND CONSTRAINT_NAME = p_constraint_name
              AND CONSTRAINT_TYPE = 'FOREIGN KEY'
        ) THEN
            SET @day1_sql = p_drop_ddl;
            PREPARE day1_stmt FROM @day1_sql;
            EXECUTE day1_stmt;
            DEALLOCATE PREPARE day1_stmt;
        END IF;

        SET @day1_sql = p_add_ddl;
        PREPARE day1_stmt FROM @day1_sql;
        EXECUTE day1_stmt;
        DEALLOCATE PREPARE day1_stmt;
    END IF;
END$$

DELIMITER ;

CALL day1_add_column_if_missing('video', 'video_type', "VARCHAR(16) NOT NULL DEFAULT 'normal' AFTER file_size");
CALL day1_add_column_if_missing('exception_log', 'event_type', "VARCHAR(32) NOT NULL DEFAULT 'person' AFTER channel_id");
CALL day1_add_column_if_missing('exception_log', 'event_end_time', 'DATETIME NULL AFTER event_time');
CALL day1_add_column_if_missing('exception_log', 'confidence', 'DECIMAL(5,4) NULL AFTER event_end_time');
CALL day1_add_column_if_missing('exception_log', 'event_status', "VARCHAR(16) NOT NULL DEFAULT 'new' AFTER confidence");
CALL day1_add_column_if_missing('feature_image', 'capture_source', "VARCHAR(16) NOT NULL DEFAULT 'manual' AFTER capture_time");
CALL day1_add_column_if_missing('feature_image', 'video_id', 'INT NULL AFTER exception_id');
CALL day1_add_column_if_missing('feature_image', 'video_offset_ms', 'BIGINT UNSIGNED NULL AFTER video_id');
CALL day1_migrate_config_key();

CALL day1_add_index_if_missing('video', 'uk_video_path', 'ALTER TABLE video ADD UNIQUE KEY uk_video_path (video_path)');
CALL day1_add_index_if_missing('video', 'idx_video_channel_start', 'ALTER TABLE video ADD KEY idx_video_channel_start (channel_id,start_time)');
CALL day1_add_index_if_missing('video', 'idx_video_type_start', 'ALTER TABLE video ADD KEY idx_video_type_start (video_type,start_time)');
CALL day1_add_index_if_missing('exception_log', 'idx_exception_channel_time', 'ALTER TABLE exception_log ADD KEY idx_exception_channel_time (channel_id,event_time)');
CALL day1_add_index_if_missing('exception_log', 'idx_exception_type_time', 'ALTER TABLE exception_log ADD KEY idx_exception_type_time (event_type,event_time)');
CALL day1_add_index_if_missing('exception_log', 'idx_exception_status_time', 'ALTER TABLE exception_log ADD KEY idx_exception_status_time (event_status,event_time)');
CALL day1_add_index_if_missing('feature_image', 'uk_feature_image_path', 'ALTER TABLE feature_image ADD UNIQUE KEY uk_feature_image_path (image_path)');
CALL day1_add_index_if_missing('feature_image', 'idx_image_channel_capture', 'ALTER TABLE feature_image ADD KEY idx_image_channel_capture (channel_id,capture_time)');
CALL day1_add_index_if_missing('feature_image', 'idx_image_video', 'ALTER TABLE feature_image ADD KEY idx_image_video (video_id)');
CALL day1_add_index_if_missing('operation_log', 'idx_operation_admin_time', 'ALTER TABLE operation_log ADD KEY idx_operation_admin_time (admin_id,operation_time)');
CALL day1_add_index_if_missing('operation_log', 'idx_operation_func_time', 'ALTER TABLE operation_log ADD KEY idx_operation_func_time (operation_func,operation_time)');

CALL day1_ensure_nullable_fk(
    'video',
    'fk_video_exception',
    'ALTER TABLE video DROP FOREIGN KEY fk_video_exception',
    'ALTER TABLE video ADD CONSTRAINT fk_video_exception FOREIGN KEY (exception_id) REFERENCES exception_log(id) ON DELETE SET NULL'
);
CALL day1_ensure_nullable_fk(
    'feature_image',
    'fk_feature_image_exception',
    'ALTER TABLE feature_image DROP FOREIGN KEY fk_feature_image_exception',
    'ALTER TABLE feature_image ADD CONSTRAINT fk_feature_image_exception FOREIGN KEY (exception_id) REFERENCES exception_log(id) ON DELETE SET NULL'
);
CALL day1_ensure_nullable_fk(
    'feature_image',
    'fk_feature_image_video',
    'ALTER TABLE feature_image DROP FOREIGN KEY fk_feature_image_video',
    'ALTER TABLE feature_image ADD CONSTRAINT fk_feature_image_video FOREIGN KEY (video_id) REFERENCES video(id) ON DELETE SET NULL'
);

INSERT INTO sys_config (config_key, config_value) VALUES
('video_dir', './records'),
('image_dir', './images'),
('record_segment_seconds', '60'),
('capture_interval_ms', '40')
ON DUPLICATE KEY UPDATE config_key = VALUES(config_key);

DROP PROCEDURE IF EXISTS day1_add_column_if_missing;
DROP PROCEDURE IF EXISTS day1_migrate_config_key;
DROP PROCEDURE IF EXISTS day1_add_index_if_missing;
DROP PROCEDURE IF EXISTS day1_ensure_nullable_fk;
