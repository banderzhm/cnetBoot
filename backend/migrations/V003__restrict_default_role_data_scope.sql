-- Before V003, data_scope=1 was stored but not enforced. Preserve the
-- previously effective owner-only access for existing non-admin roles.
UPDATE sys_role
SET data_scope = 5
WHERE data_scope = 1 AND role_key <> 'admin';

ALTER TABLE sys_role
    MODIFY COLUMN data_scope INT NOT NULL DEFAULT 5;
