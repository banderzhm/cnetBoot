-- V008 —— 把 chart:* / chat:* 权限码授予 admin 与 practitioner。
--
-- 为什么单独一个迁移：V005/V006/V007 的授权语句在迁移期执行，而本项目
-- 不在迁移里 seed 角色（sys_role 由首次启动的 bootstrap 建立）。所以那三条
-- INSERT IGNORE 当时匹配不到任何角色，授出 0 行 —— 权限码存在、角色存在，
-- 但两者没有关联，任何角色访问 chart/chat 路由都会 403。
--
-- 本迁移可以安全地在角色建立之后再跑一次：它只补缺失的关联，不删除、不重复。
-- 幂等：sys_role_menu 主键是 (role_id, menu_id)，INSERT IGNORE 不会重复插入。

INSERT IGNORE INTO sys_role_menu (role_id, menu_id)
SELECT r.role_id, m.menu_id
FROM sys_role r
JOIN sys_menu m ON m.permission LIKE 'chart:%' OR m.permission LIKE 'chat:%'
WHERE r.role_key IN ('admin', 'practitioner');
