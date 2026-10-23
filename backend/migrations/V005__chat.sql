
--






--



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




INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, 'List conversations', '', '', 'chat:session:list', 'F', '', 1, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:list');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, 'Create conversation', '', '', 'chat:session:create', 'F', '', 2, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:create');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, 'Update conversation', '', '', 'chat:session:update', 'F', '', 3, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:update');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, 'Delete conversation', '', '', 'chat:session:delete', 'F', '', 4, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:session:delete');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, 'Send message', '', '', 'chat:message:send', 'F', '', 5, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chat:message:send');

INSERT IGNORE INTO sys_role_menu (role_id, menu_id)
SELECT r.role_id, m.menu_id
FROM sys_role r
JOIN sys_menu m ON m.permission LIKE 'chat:%'
WHERE r.role_key = 'admin';
