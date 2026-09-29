-- V007 —— 补齐 chart 业务路由实际声明、但 V005/V006 未曾种下的权限码。
--
-- 背景：chart 控制器在路由注册处声明了 5 个权限码，而 V005 只种了 case 的
-- create/list/update/delete 与 archive:list，V006 只种了 archive:list 一类。
-- 于是 /api/v1/tags、/api/v1/charts/batch、/api/v1/cases/:id/reading、
-- /api/v1/cases/:id（读单个）与 DELETE /api/v1/archive/... 在真实授权下对
-- 任何角色都返回 403 —— 路由与守卫工作正常，缺的是授权数据。
--
-- 幂等：INSERT ... ON DUPLICATE KEY UPDATE + INSERT IGNORE。
-- 权限码挂在 sys_menu.permission（本项目菜单即权限模型），menu_type='F'、
-- visible=0，授予 admin 与 practitioner。

-- sys_menu 上没有 permission 唯一键，改用 WHERE NOT EXISTS 守卫，保证迁移
-- 重复执行不堆积行。
INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '标签管理', '', '', 'chart:tag:manage', 'F', '', 6, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:tag:manage');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '批量排盘', '', '', 'chart:batch:run', 'F', '', 7, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:batch:run');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '案例断语', '', '', 'chart:reading:run', 'F', '', 8, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:reading:run');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '案例详情', '', '', 'chart:case:read', 'F', '', 9, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:case:read');

INSERT INTO sys_menu (parent_id, menu_name, path, component, permission, menu_type, icon, order_num, visible, status)
SELECT 0, '删除存档', '', '', 'chart:archive:delete', 'F', '', 10, 0, 0
WHERE NOT EXISTS (SELECT 1 FROM sys_menu WHERE permission = 'chart:archive:delete');

INSERT IGNORE INTO sys_role_menu (role_id, menu_id)
SELECT r.role_id, m.menu_id
FROM sys_role r
JOIN sys_menu m ON m.permission IN ('chart:tag:manage', 'chart:batch:run',
    'chart:reading:run', 'chart:case:read', 'chart:archive:delete')
WHERE r.role_key IN ('admin', 'practitioner');
