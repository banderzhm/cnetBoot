CREATE TABLE IF NOT EXISTS audit_log (
    id BIGINT NOT NULL AUTO_INCREMENT,
    actor_user_id BIGINT NOT NULL DEFAULT 0,
    actor_username VARCHAR(64) NOT NULL DEFAULT '',
    source_ip VARCHAR(64) NOT NULL DEFAULT '',
    user_agent VARCHAR(512) NOT NULL DEFAULT '',
    request_id VARCHAR(128) NOT NULL DEFAULT '',
    request_method VARCHAR(16) NOT NULL DEFAULT '',
    request_path VARCHAR(512) NOT NULL DEFAULT '',
    action_name VARCHAR(32) NOT NULL,
    resource_name VARCHAR(64) NOT NULL,
    status_code INT NOT NULL,
    detail TEXT NOT NULL,
    created_at DATETIME(6) NOT NULL,
    PRIMARY KEY (id),
    KEY idx_audit_actor_created (actor_user_id, created_at),
    KEY idx_audit_resource_created (resource_name, created_at),
    KEY idx_audit_request_id (request_id)
);
