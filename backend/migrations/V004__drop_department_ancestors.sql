-- V004 —— 部门层级与数据范围改用 parent_id，ancestors 冗余，予以删除。
--
-- 幂等：列不存在时跳过，重复执行不报 Can't DROP COLUMN。

SET @schema_name = DATABASE();
SET @have_ancestors = (SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = @schema_name AND TABLE_NAME = 'sys_dept' AND COLUMN_NAME = 'ancestors');
SET @sql = IF(@have_ancestors > 0,
    'ALTER TABLE sys_dept DROP COLUMN ancestors',
    'DO 0');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
