# 实施报告：架构优化 + router 拆分 + 多供应商 LLM

目标树：`E:\github\cnetNexus\backend`（Git 跟踪的正式树；`backend_master` 是逐字节相同的副本，未改动）。
构建/测试全部在 WSL 完成，clang 22 + cmake 4.4.3，**真实 MariaDB 12.3 + Valkey**。

## 零、第二轮复核修复（P0 崩溃 + P1 白名单）

### P0：`open_orm()` 的 use-after-move —— 真实 MySQL 下确认段错误

第一轮把 `orm_scope` 写成聚合体 + 花括号初始化：

```cpp
return orm_scope{std::move(lease),
    raw_session{lease.get(), sql_dialect::mysql}};   // lease 已被移空
```

聚合初始化按成员顺序求值：第一个成员先把入参移空，之后才求值 `lease.get()`，
而 cnetmod 的 `get()` 是 `*node_->conn` 且移动构造用 `std::exchange(node_, nullptr)`
（`mysql_pool_impl.cpp:52-54, 78-81`），于是**当场解引用空指针**。

**真实 MySQL 下的 A/B 实证**（同一份测试、同一棵树，只改成员初始化顺序）：

| 实现 | 结果 |
|---|---|
| 旧顺序（成员先移动、再对入参取地址） | **EXIT=139（SIGSEGV）** |
| 修复后（先构造成员、再对成员取地址） | EXIT=0，6/6 断言通过 |

修复方式：把「租约 + 会话」的绑定抽成导出的 `mysql_lease_scope`
（`modules/chart/mapper/mysql_lease_scope.cppm`），手写构造函数在成员初始化列表里
先初始化 `lease_`，再 `session_{lease_.get(), ...}`；11 个调用点改用 `scope->connection()`，
不再接触内部结构。新增 `testing/chart_mysql_integration.cpp` 直连真实 MySQL 覆盖这条路径。

### P1：白名单不区分 HTTP 方法 —— 删除接口对所有人 401

`public_path_middleware` 命中白名单时直接 `next()`，而白名单**只比路径不比方法**。
`/api/v1/archive/:algorithm/:id` 的 GET 是已发布公开契约、DELETE 需权限，
逐算法前缀把两者一起放开 → 令牌不被解析 → 权限守卫拿不到身份 → **删除功能整个不可用**。

修复：`route_descriptor` 输出 `public_path_entry{method, prefix}`，
`matches_public_path(method, path, entries)` 按「方法 + 路径」匹配。

逐条判定真实路由的修复效果（只有该变的变了）：

| 方法 | 路径 | 修复前 | 修复后 | 说明 |
|---|---|---|---|---|
| GET | /api/v1/archive/ba_zi/abc | 公开 | 公开 | 已发布公开契约，保持不变 |
| **DELETE** | **/api/v1/archive/ba_zi/abc** | **公开** | **鉴权** | **← 本次修复** |
| GET | /api/v1/archive/search | 鉴权 | 鉴权 | 保持不变 |
| POST | /api/v1/bazi/archive | 公开 | 公开 | 已发布公开契约 |
| GET | /api/v1/cases | 鉴权 | 鉴权 | 保持不变 |
| GET | /api/v1/bazi/chart | 公开 | 公开 | 已发布公开契约 |

同类修复：`route_registry::dispatch` 遇到未知方法名原本会**静默丢掉**整条路由，
现在抛 `std::invalid_argument`（路由凭空消失比明确报错难查得多）。

### P1b：公开 ≠ 匿名（可选鉴权）

公开路径跳过令牌解析会带来两个后果：存档 GET 读不回带身份写入的数据
（batch save_archive 记的是用户 id，GET 只能看到匿名分区）；reading 缓存永远按匿名分区。
新增 `security::optional_jwt_security`：公开路径上**尝试**解析 Bearer 并绑定身份，
缺令牌/令牌失效一律放行——公开契约不会因令牌问题返回 401。
校验逻辑抽成具名 `authenticate_request`，与严格中间件共用一份，避免两条路径语义漂移。

## 一、验证证据（干净树从零构建）

| 项目 | 证据 |
|---|---|
| 完整干净构建 | CONFIGURE_EXIT=0、BUILD_EXIT=0、`[1661/1661] Linking CXX executable cnetnexus_server`、**0 编译错误**、0 个 `uninitialized` 警告 |
| 测试 | **34/34 全绿**（基线 28 + 新增 6） |
| 真实 MySQL | `chart_mysql_integration`：**6/6 断言**，覆盖租约绑定与移动后有效性 |
| 路由层 | `framework_route_registry`：**33/33 断言**，含方法隔离与可选鉴权 |
| 路由表零回归 | 基线 44 条 → 46 条，**MISSING = 空**；`/archive/search`(1) 早于通配(45)，字面量优先 OK |
| 匿名内部类 | 路由注册处内联 lambda：**0** |

## 二、router 功能单独拆分

- `framework/web/route_registry.cppm/.cpp`：`route_descriptor{method, path, handler, visibility, public_prefixes}`、
  `route_registry`（`get/post/put/del/patch/add` + `descriptors/size/public_entries/mount_into`）、
  具名适配器 `route_handler<Dependency>` / `stateless_route_handler`
- `framework/web/route_manifest.cppm/.cpp`：白名单延迟绑定代理 + `matches_public_path(method, path, entries)` + 可选鉴权

**关于「唯一事实来源」的准确表述**：免鉴权集合不再有第二份手写清单，
它由路由声明里的 `route_visibility::public_` 经 `public_entries()` 派生。
第一轮我把这句说过头了——当时派生结果**丢掉了 HTTP 方法**，所以「唯一来源」
是对的，但派生规则错了，等于把旧缺陷原样搬了过来。现在方法参与派生与匹配。

17 个控制器、18 个声明式注册器；`register_*` 遗留 0 个。

## 三、不准匿名内部类

路由注册处 lambda 归零；系统控制器改为具名依赖结构 + 具名自由函数；
新增类型全部具名导出（`route_descriptor`、`route_registry`、`route_visibility`、
`public_path_entry`、`public_path_middleware`、`mysql_lease_scope`）。

## 四、多供应商 / 多模型 / 多 key

三层分离，选择策略可脱离网络单测：

1. `framework/config/llm_settings.cppm/.cpp` —— 纯逻辑：目录投影、默认值回退、多 key 轮询、失败切 key、加载期校验。
2. `modules/reading/service/provider_llm_service.cppm/.cpp` —— 编排：首调用 + 最多 2 次换 key；首帧后不换 key。
3. `modules/reading/client/chat_model_invocation.cppm/.cpp` —— 唯一一次真调用。

实测：cnetmod 的 openai 服务按 `(name, instance)` 注册，`pool_size` **不做 key 轮换**，
所以「一个 key = 一个 services 实例」，再由 `llm.providers[].keys[]` 聚合成逻辑供应商。
3 实例 / 2 供应商配置已通过加载期校验；`llm_provider_selection` **56 检查点 0 失败、完全不联网**。
端点 `GET /api/v1/llm/providers` 只回脱敏信息。

## 五、代码优化

| 缺陷 | 修复 |
|---|---|
| **`open_orm` use-after-move（段错误）** | 抽出具名 `mysql_lease_scope` + 手写构造函数；11 个调用点改用访问器 |
| **跨租户越权 IDOR** | 具名 `archive_access` / `archive_owner_filter`；索引新增 `find()`；XML/内存/文件扫描同口径谓词 |
| **白名单不区分方法** | `public_path_entry{method, prefix}` + 方法参与匹配 |
| **公开路径不绑定身份** | `optional_jwt_security`：尝试解析 Bearer，失败放行 |
| **无请求体上限** | 安装 `body_limit(1MB)`（实测 ba_zi 1.13ms/盘，32MB ≈ 10 分钟同步 CPU） |
| **batch 无上限** | `max_batch_items = 100`，超限 400；用例用**时序**证明超限请求未进排盘循环 |
| **存档删除顺序** | 改为先索引后正文（与写入反向），正文失败时补偿索引；14 断言覆盖三个方向 |
| **测试代码污染生产库** | 拆出 `cnetnexus_chart_testing`；符号级证据：生产库/服务器 0 个，测试库 122 个 |
| **`dispatch` 静默丢路由** | 未知方法名抛异常 |

新增用例：`chart_mysql_integration`、`framework_route_registry`、`chart_archive_ownership`、
`chart_batch_limit`、`chart_archive_delete_order`、`llm_provider_selection`。

## 六、过程中修掉/纠正的问题

1. **我上一轮的判断错了**：我曾以「11 个调用点都用 `scope->lease.get()`」为由，
   把 P0 从「会崩溃」降级为「沉默陷阱」。真实 MySQL A/B 证明**构造函数本身**就段错误
   （`EXIT=139`），与调用点怎么用返回值无关。你的原始判断是对的。
2. 我自己的回归用例抓出补偿路径只覆盖了「归属查询执行过」的分支，已改为总是先读索引行。
3. LLM 配置加载曾放在 `configuration_file` 被 move 之后，路径成空串导致多供应商永不生效。
4. 配置样例的 `${VAR:-default}` 不受支持（cnetmod 只认 `${VAR}`，变量缺失会让整份配置加载失败）。

## 七、遗留

**已确认与本次改动无关**：服务器不监听 8080 —— 纯净 `backend_master` 基线行为完全相同，
是既有环境问题（`business_server.listen()` 失败时只设 run_error 就返回，不打印也不退出）。
因此 P1 的**运行时**验证改用逐条判定脚本 + 单元测试完成，不是真机 curl。

**尚未实施**：统一错误响应格式（线上是 `{"code":"<HTTP字符串>","status":"<注册码>"}`，
原评审写反了；且删除 `common/api_response` 会引入反向依赖）；鉴权查询 3→2 条 + Redis pipeline；
principal 缓存（失效面尚不存在）；`llm_selector` 轮询游标无锁（单事件循环下安全）。
