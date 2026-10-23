
--



SET @schema_name = DATABASE();


SET @have_dept_id = (SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = @schema_name AND TABLE_NAME = 'sys_user' AND COLUMN_NAME = 'dept_id');
SET @sql = IF(@have_dept_id = 0,
    'ALTER TABLE sys_user ADD COLUMN dept_id BIGINT NOT NULL DEFAULT 0 AFTER user_id',
    'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @have_dept_ix = (SELECT COUNT(*) FROM information_schema.STATISTICS
    WHERE TABLE_SCHEMA = @schema_name AND TABLE_NAME = 'sys_user' AND INDEX_NAME = 'ix_sys_user_dept');
SET @sql = IF(@have_dept_ix = 0,
    'ALTER TABLE sys_user ADD KEY ix_sys_user_dept (dept_id)',
    'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- sys_role.data_scope
SET @have_data_scope = (SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = @schema_name AND TABLE_NAME = 'sys_role' AND COLUMN_NAME = 'data_scope');
SET @sql = IF(@have_data_scope = 0,
    'ALTER TABLE sys_role ADD COLUMN data_scope INT NOT NULL DEFAULT 1 AFTER role_key',
    'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

CREATE TABLE IF NOT EXISTS sys_dept (
    dept_id BIGINT NOT NULL AUTO_INCREMENT,
    parent_id BIGINT NOT NULL DEFAULT 0,
    ancestors VARCHAR(500) NOT NULL DEFAULT '',
    dept_name VARCHAR(64) NOT NULL,
    order_num INT NOT NULL DEFAULT 0,
    leader VARCHAR(64) NOT NULL DEFAULT '',
    email VARCHAR(128) NOT NULL DEFAULT '',
    phone VARCHAR(32) NOT NULL DEFAULT '',
    status TINYINT NOT NULL DEFAULT 0,
    deleted TINYINT NOT NULL DEFAULT 0,
    PRIMARY KEY (dept_id),
    KEY ix_sys_dept_parent (parent_id)
);

CREATE TABLE IF NOT EXISTS sys_role_dept (
    role_id BIGINT NOT NULL,
    dept_id BIGINT NOT NULL,
    PRIMARY KEY (role_id, dept_id),
    KEY ix_sys_role_dept_dept (dept_id)
);
