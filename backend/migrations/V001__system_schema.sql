CREATE TABLE IF NOT EXISTS sys_user (
    user_id BIGINT NOT NULL AUTO_INCREMENT,
    user_name VARCHAR(64) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    nick_name VARCHAR(64) NOT NULL,
    email VARCHAR(128) NOT NULL DEFAULT '',
    status TINYINT NOT NULL DEFAULT 0,
    deleted TINYINT NOT NULL DEFAULT 0,
    version BIGINT NOT NULL DEFAULT 0,
    created_at DATETIME(6) NOT NULL,
    updated_at DATETIME(6) NOT NULL,
    PRIMARY KEY (user_id),
    UNIQUE KEY uk_sys_user_name (user_name)
);

CREATE TABLE IF NOT EXISTS sys_role (
    role_id BIGINT NOT NULL AUTO_INCREMENT,
    role_name VARCHAR(64) NOT NULL,
    role_key VARCHAR(100) NOT NULL,
    role_sort INT NOT NULL DEFAULT 0,
    status TINYINT NOT NULL DEFAULT 0,
    deleted TINYINT NOT NULL DEFAULT 0,
    version BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (role_id),
    UNIQUE KEY uk_sys_role_key (role_key)
);

CREATE TABLE IF NOT EXISTS sys_menu (
    menu_id BIGINT NOT NULL AUTO_INCREMENT,
    parent_id BIGINT NOT NULL DEFAULT 0,
    menu_name VARCHAR(64) NOT NULL,
    path VARCHAR(255) NOT NULL DEFAULT '',
    component VARCHAR(255) NOT NULL DEFAULT '',
    permission VARCHAR(128) NOT NULL DEFAULT '',
    menu_type VARCHAR(1) NOT NULL,
    icon VARCHAR(100) NOT NULL DEFAULT '',
    order_num INT NOT NULL DEFAULT 0,
    visible TINYINT NOT NULL DEFAULT 0,
    status TINYINT NOT NULL DEFAULT 0,
    PRIMARY KEY (menu_id),
    KEY ix_sys_menu_parent (parent_id)
);

CREATE TABLE IF NOT EXISTS sys_user_role (
    user_id BIGINT NOT NULL,
    role_id BIGINT NOT NULL,
    PRIMARY KEY (user_id, role_id),
    KEY ix_sys_user_role_role (role_id)
);

CREATE TABLE IF NOT EXISTS sys_role_menu (
    role_id BIGINT NOT NULL,
    menu_id BIGINT NOT NULL,
    PRIMARY KEY (role_id, menu_id),
    KEY ix_sys_role_menu_menu (menu_id)
);
