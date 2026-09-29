-- V005 —— 通用 AI 多轮对话、会话消息与 chat:* 权限码。
--
-- 口径：
--   * 会话与消息都是逻辑删除（deleted_at），读取一律排除非空行；
--   * chat_message.session_id 建索引，会话消息分页与「最近 N 条」都走它；
--   * message_count / last_message_at 是会话上的冗余计数，由 mapper 在追加消息时维护；
--   * 权限码挂在 sys_menu.permission（本项目的菜单即权限模型），menu_type='F' 表示按钮；
--   * 幂等：CREATE TABLE IF NOT EXISTS + INSERT ... ON DUPLICATE KEY UPDATE。
--
-- 时间口径：DATETIME(6) 一律存 UTC 墙钟；SQL 里不出现 NOW()、UNIX_TIMESTAMP 等按会话时区
-- 解释的函数，由应用在 mapper 边界写入 UTC 值。

CREATE TABLE IF NOT EXISTS chat_session (
    id              BIGINT NOT NULL AUTO_INCREMENT,
    owner_id        BIGINT NOT NULL DEFAULT 0,
    title           VARCHAR(128) NOT NULL DEFAULT '',
    category        VARCHAR(64) NOT NULL DEFAULT '',
    system_prompt   TEXT NULL,
    model           VARCHAR(64) NOT NULL DEFAULT '',
    temperature     DECIMAL(3,2) NOT NULL DEFAULT 0.70,
    message_count   INT NOT NULL DEFAULT 0,
    last_message_at DATETIME(6) NULL,
    created_by      BIGINT NOT NULL DEFAULT 0,
    updated_by      BIGINT NOT NULL DEFAULT 0,
    created_at      DATETIME(6) NOT NULL,
    updated_at      DATETIME(6) NOT NULL,
    deleted_at      DATETIME(6) NULL,
    PRIMARY KEY (id),
    KEY ix_chat_session_owner (owner_id, created_at),
    KEY ix_chat_session_category (category),
    KEY ix_chat_session_updated (updated_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS chat_message (
    id         BIGINT NOT NULL AUTO_INCREMENT,
    session_id BIGINT NOT NULL,
    role       VARCHAR(16) NOT NULL,
    content    MEDIUMTEXT NOT NULL,
    model      VARCHAR(64) NOT NULL DEFAULT '',
    tokens_in  INT NOT NULL DEFAULT 0,
    tokens_out INT NOT NULL DEFAULT 0,
    created_at DATETIME(6) NOT NULL,
    deleted_at DATETIME(6) NULL,
    PRIMARY KEY (id),
    KEY ix_chat_message_session (session_id, id),
    KEY ix_chat_message_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- chat:* 权限码。sys_menu 上没有 permission 唯一键，因此 ON DUPLICATE KEY
-- UPDATE 永不触发；改用 WHERE NOT EXISTS 守卫，保证迁移可重复执行而不堆积行。
-- menu_type='F' 表示按钮权限，visible=0 表示不进侧栏。
INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '查询会话', '', '', 'chat:session:list', 'F', '', 1, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:list');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '新建会话', '', '', 'chat:session:create', 'F', '', 2, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:create');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '修改会话', '', '', 'chat:session:update', 'F', '', 3, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:update');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '删除会话', '', '', 'chat:session:delete', 'F', '', 4, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:delete');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '发送消息', '', '', 'chat:message:send', 'F', '', 5, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:message:send');

INSERT IGNORE INTO sys_role_menu (role_id, menu_id)
SELECT r.role_id, m.menu_id
FROM sys_role r
JOIN sys_menu m ON m.permission LIKE 'chat:%'
WHERE r.role_key = 'admin';
