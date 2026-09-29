-- V006 —— 命理师工作台：排盘存档索引、案例库、标签与案例-标签关联。
--
-- 口径（与 chart 模块的 ORM 模型逐列对照）：
--   * 时间列一律 DATETIME(6)，存 UTC 墙钟；SQL 里不出现 NOW()/UNIX_TIMESTAMP()，
--     由 mapper 边界把 unix 秒转成 calendar_datetime 参数写入；
--   * chart_case / chart_tag 为逻辑删除或按 owner 隔离，读取一律排除 NULL 标记；
--   * chart_archive_index 初始索引结构；V009 将正文迁入同表；
--   * 幂等：CREATE TABLE IF NOT EXISTS。

CREATE TABLE IF NOT EXISTS chart_archive_index (
    archive_id CHAR(16) NOT NULL,
    algorithm  VARCHAR(32) NOT NULL,
    title      VARCHAR(128) NOT NULL DEFAULT '',
    owner_id   BIGINT NOT NULL DEFAULT 0,
    size_bytes INT UNSIGNED NOT NULL DEFAULT 0,
    created_at DATETIME(6) NOT NULL,
    PRIMARY KEY (archive_id),
    KEY ix_archive_algo_created (algorithm, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS chart_case (
    id           BIGINT NOT NULL AUTO_INCREMENT,
    owner_id     BIGINT NOT NULL,
    title        VARCHAR(128) NOT NULL,
    algorithm    VARCHAR(32) NOT NULL,
    archive_id   CHAR(16) NULL,
    calendar     VARCHAR(16) NOT NULL DEFAULT 'solar',
    gender       VARCHAR(8) NOT NULL DEFAULT '',
    birth_year   SMALLINT NOT NULL DEFAULT 0,
    birth_month  TINYINT NOT NULL DEFAULT 0,
    birth_day    TINYINT NOT NULL DEFAULT 0,
    birth_hour   TINYINT NOT NULL DEFAULT 0,
    birth_minute TINYINT NOT NULL DEFAULT 0,
    birth_place  VARCHAR(128) NOT NULL DEFAULT '',
    client_name  VARCHAR(64) NOT NULL DEFAULT '',
    question     VARCHAR(512) NOT NULL DEFAULT '',
    notes        MEDIUMTEXT NULL,
    reading      MEDIUMTEXT NULL,
    payload      MEDIUMTEXT NULL,
    created_by   BIGINT NOT NULL DEFAULT 0,
    updated_by   BIGINT NOT NULL DEFAULT 0,
    created_at   DATETIME(6) NOT NULL,
    updated_at   DATETIME(6) NOT NULL,
    deleted_at   DATETIME(6) NULL,
    PRIMARY KEY (id),
    KEY ix_case_owner (owner_id, created_at),
    KEY ix_case_algo (algorithm),
    KEY ix_case_archive (archive_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS chart_tag (
    id         BIGINT NOT NULL AUTO_INCREMENT,
    name       VARCHAR(32) NOT NULL,
    color      VARCHAR(16) NOT NULL DEFAULT '#1677ff',
    owner_id   BIGINT NOT NULL DEFAULT 0,
    created_at DATETIME(6) NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_chart_tag_name (name, owner_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS chart_case_tag (
    case_id BIGINT NOT NULL,
    tag_id  BIGINT NOT NULL,
    PRIMARY KEY (case_id, tag_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- chart:* 权限码，用 WHERE NOT EXISTS 守卫保证可重复执行（sys_menu 无
-- permission 唯一键）。
INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '存档查询', '', '', 'chart:archive:list', 'F', '', 1, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:archive:list');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '案例查询', '', '', 'chart:case:list', 'F', '', 2, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:case:list');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '案例新增', '', '', 'chart:case:create', 'F', '', 3, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:case:create');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '案例修改', '', '', 'chart:case:update', 'F', '', 4, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:case:update');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '案例删除', '', '', 'chart:case:delete', 'F', '', 5, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:case:delete');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '排盘生成', '', '', 'chart:generate', 'F', '', 6, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:generate');

INSERT IGNORE INTO sys_role_menu (role_id, menu_id)
SELECT r.role_id, m.menu_id
FROM sys_role r
JOIN sys_menu m ON m.permission LIKE 'chart:%'
WHERE r.role_key IN ('admin', 'practitioner');
