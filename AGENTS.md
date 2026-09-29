# cnetBoot Repository Instructions

本文件是 cnetBoot 的项目级规则。它只描述 cnetBoot 的产品边界、分层、命名、组合和验收要求；
cnetmod 的 API、协程、ORM、Redis、HTTP、SSE、安全、AI、模块与构建规则，以 cnetmod 仓库
最新生成的 `AGENTS.md` 为唯一事实来源。

## 0. AI 开始工作前的强制步骤

如果执行者是 AI Agent，开始搜索、分析、设计、修改或生成代码之前，必须依次完成：

1. 完整读取本文件。
2. 定位 cnetmod 源码仓库。当前标准工作区布局为：
   - cnetBoot：`E:\github\cnetBoot`
   - cnetmod：`E:\github\cnetmod`
   - 从 cnetBoot 仓库使用相对路径时为 `..\cnetmod\AGENTS.md`。
3. **完整读取最新的 `E:\github\cnetmod\AGENTS.md`，不得只读取摘要、节选或凭旧上下文回忆。**
4. 根据任务类型，再读取 cnetmod `AGENTS.md` 中列出的对应 `skill/**/*.md` 源文件。例如：
   - Application、组件注册、多事件循环：`skill/infra/application.md`
   - 架构、模块、接口实现分离：`skill/infra/architecture.md`、
     `skill/infra/module-conventions.md`、`skill/infra/new-module-guide.md`
   - ORM、XML、数据权限、多租户、自动填充：`skill/database/database-orm.md`
   - MySQL / Redis：`skill/database/mysql.md`、`skill/database/redis.md`
   - HTTP 路由、中间件、SSE：`skill/http/http-server.md`、`skill/http/http-middleware.md`
   - JWT 与口令安全：`skill/security/security-jwt.md`
   - AI / OpenAI 接入：`skill/protocols/openai-mail-dns.md`
   - 协程、执行器、超时和取消：`skill/coro/coroutine.md`、
     `skill/coro/executor-bridge.md`、`skill/coro/timer-retry.md`
5. 检查实际安装或引用的 cnetmod 版本与源码分支，不能因为“API 看起来相似”而猜签名。

若 cnetmod 仓库或其 `AGENTS.md` 不存在，AI 必须明确报告缺失事实；不得自行补造 cnetmod API、
复制旧实现或在 cnetBoot 内重造一套框架能力。若本文件与 cnetmod 规则冲突：

- cnetmod API、安全、并发、模块和底层能力由 cnetmod `AGENTS.md` 决定；
- cnetBoot 的业务边界、目录分层、命名与组合方式由本文件决定；
- 仍无法同时满足时，停止实现并把具体冲突交给用户决定。

不要把 cnetmod 的 60 多万字生成文档复制进本仓库。必须读取上游最新文件，避免两份规范漂移。

## 1. 产品边界

cnetBoot 是基于 cnetmod 的通用 C++23 后端基础脚手架，不是任何具体行业应用。

内置能力只能包括：

- RBAC：用户、角色、菜单/权限、部门、数据范围；
- 身份：登录、JWT 签发/验证、Redis 会话、刷新、撤销、登录防爆破；
- 基础设施：统一错误、分页、审计、限流、请求体限制、SSE、可观测性；
- 通用 AI：多供应商、多模型、多 Key、显式模型注册、目录查询；
- 通用 Chat：会话、消息、分类、参考上下文、同步与流式生成。

禁止把任何垂直业务直接写入基础模板，包括但不限于：

- 行业算法、行业实体、行业提示词、行业缓存键或行业错误码；
- 为某个业务定制的路由、表、枚举、DTO、权限码和默认角色；
- 通过 `if/else`、字符串类型码或一个“大而全 Service”隐藏多个业务模块。

需要新增业务时，创建 `backend/modules/<bounded-context>/`，由自己的 Application module 显式
注册。业务模块可以依赖 system、ai、chat 的公开契约，基础模块不得反向依赖新增业务。

## 2. 总体架构与依赖方向

内置 Application 模块固定为：

```text
platform -> identity -> ai -> chat
```

- `platform`：入口中间件、统一 404/405、XML 语句集、审计存储等跨业务基础能力；
- `identity`：RBAC、认证、授权、JWT/Redis 会话与 system 路由；
- `ai`：typed options、模型目录、显式 `add_chat_model`、应用层 `llm_service`；
- `chat`：会话/消息仓储、Chat 用例和 HTTP/SSE 路由。

唯一组合根是 `backend/bootstrap/`。只有组合根可以：

- 读取顶层配置并声明 typed options；
- 注册 repository、mapper、service、model 和 middleware；
- 决定实现绑定、实例名、策略和生命周期；
- 连接不同 bounded context。

业务层禁止直接依赖 `application_runtime`，禁止把 runtime 当 service locator，禁止先构造空对象再
`attach()` 的两阶段初始化。依赖必须通过构造函数或明确的 factory 注入；构建阶段缺依赖必须让
Application build 失败，而不是等首个请求才失败。

## 3. Spring 风格模块分层

每个业务模块使用同级目录：

```text
modules/<context>/
  controller/  HTTP 路由声明和协议适配
  dto/         请求/响应 DTO、领域对象到传输对象的投影
  service/     具名用例、事务边界、业务编排
  domain/      实体、值对象、领域规则、ORM record
  mapper/      Repository 实现、XML statement、持久化转换
```

依赖方向：

```text
controller + dto -> service -> domain <- mapper
bootstrap -> 所有层（仅装配）
```

具体约束：

- Controller 只解析路径、查询、Header、Body，调用一个清晰用例并写响应；不得写 SQL、权限算法、
  缓存策略或跨多个底层组件自行编排业务。
- DTO 必须位于与 controller 同级的 `dto/`；禁止 `controller/*_dto.cppm`。
- ORM record 是数据库实体，必须位于 `domain/`；禁止放在 mapper 中。
- Mapper 只处理持久化查询、ORM/XML 参数转换和 record/domain 映射；不得持有 runtime。
- Service 以业务用例或领域能力命名，例如 `authorization_service`、`chat_service`、
  `login_guard`。禁止 `batch_service`、`common_service`、`base_service`、`manager_service`、
  `helper_service` 等无法说明业务所有权的名称。
- 跨模块通用的 HTTP、时间、安全、审计能力放在 `backend/framework/`；不能借“复用”为由让一个
  业务 controller 成为其它模块的公共库。
- `.cppm` 只导出接口；非模板实现放 `.cpp`。不要把实现全部塞回模块接口文件。

## 4. cnetmod 能力优先，禁止应用层造轮子

实现前必须先在最新 cnetmod `AGENTS.md`、skill 和源码中确认已有能力。以下场景必须优先使用
cnetmod：

- Application builder、application_module、typed options、component resolver；
- repository factory、managed repository、ORM wrapper、XML mapper、逻辑删除、自动填充；
- 数据权限、多租户、租户树/组织树上下文；
- Redis template、连接池超时、pipeline；
- JWT 中间件、端点元数据、optional authentication、permission authorization；
- HTTP router、统一 405、body limit、rate limiter；
- `request_context::with_sse()`、SSE writer、取消、超时和事件循环恢复；
- `add_chat_model`、composed model、模型调用、重试/熔断基础能力；
- 文件异步 I/O、durability、executor/offload。

只有确认 cnetmod 不具备通用能力时，才允许在 cnetBoot 中实现应用特有部分。若缺口属于通用框架，
应修 cnetmod，并同步更新它的 skill 与生成后的 `AGENTS.md`；不要在 cnetBoot 写 resolver、wrapper、
service locator 或字符串匹配补丁来绕开。

## 5. AI 与 Chat 规则

- AI 能力必须显式开启。`ai_module` 读取并验证 `llm` typed options，然后显式调用
  `add_chat_model`；仅出现配置或路由不得隐式创建默认模型。
- 多供应商、多模型、多 Key 是应用组合策略。cnetmod 不负责业务意义上的换 Key；cnetBoot 通过
  显式 composed model 配置 round-robin、fallback 和每 Key 尝试次数。
- Provider SDK、`cnetmod.ai` 和上游请求/响应类型只能出现在 `modules/ai/service/model/` 边界。
- 其它模块只依赖 cnetBoot 的 `llm_service`、`llm_request`、`llm_response` 契约。
- Chat 的 `category` 是应用自定义分类，`context` 是通用参考上下文；不能用它们偷偷编码某个行业。
- 系统提示词必须通用，不能内置特定行业角色、术语和结论格式。
- AI 请求失败使用注册错误码，不能靠匹配厂商错误字符串决定业务状态。
- 流式接口必须从 `request_context::with_sse()` 进入，以保留总时长和单次写入看门狗。
- 下游断开、请求取消、上游 429/5xx 是不同事实；不得把“连接关闭”描述成“上游一定停止生成”。

## 6. JSON 与 DTO

- 禁止手写或拼接原始 JSON 字符串，包括示例请求、错误兜底和测试期望。
- 请求与响应先定义 DTO；稳定响应通过 DTO 投影，动态结构使用 `cnetmod::json::object()`、
  `array()` 和逐级字段赋值。
- JSON 编解码只使用 cnetmod 提供的 Glaze-only 工具；不得引入第二套 JSON 框架。
- DTO 字段名由传输契约决定，领域字段名由领域语义决定；转换集中在 dto/mapper 边界。
- 不得在 Controller 中散落重复的 JSON key、时间格式化或 nullable 编码逻辑。

## 7. 数据库、ORM、XML 与迁移

- 优先 ORM repository/wrapper；需要动态 SQL 或框架明确不支持的数据库语义时才使用 XML。
- ORM record 放 domain，repository/mapper 构造注入；禁止 mapper 自己从 runtime 取连接或仓储。
- ORM 与 XML 必须共享相同的租户、数据权限、逻辑删除和自动填充语义。
- `deleted` 0/1 与 nullable `deleted_at` 等策略按实体/表配置，不允许全局策略互相污染。
- `created_at` / `updated_at` 使用 cnetmod 自动填充；确需保留外部时间或多列原子同值时明确说明。
- MySQL `DATETIME(6)` 统一保存 UTC 墙钟；Unix 秒与 `calendar_datetime` 只在 mapper 边界转换；
  禁止依赖数据库会话时区的 `NOW()`、`CURRENT_TIMESTAMP`、`FROM_UNIXTIME()`。
- XML 语句不得重复手写已由拦截器注入的租户、数据权限或逻辑删除条件。
- 所有 schema 变化必须新增按序迁移文件；不要修改已经发布的迁移来伪造历史。
- 唯一键冲突、数据库错误等必须使用 cnetmod 提供的结构化错误信息，不得解析错误消息文本。
- 数据库相关改动至少补 mapper/SQL 渲染测试；复杂租户、上下级、逻辑删除和自动填充需要真实数据库测试。

## 8. 身份、安全与 Redis

- JWT issuer、secret、有效期、会话空闲期必须来自 YAML/env typed configuration，禁止硬编码。
- JWT 只是会话凭证；每个受保护请求必须核验 Redis 会话状态，logout/refresh 后撤销应立即生效。
- Redis 保存会话 key 与用户会话信息，刷新/访问按策略续期；不得把“能解 JWT”当成“会话有效”。
- Redis 获取连接必须受 pool timeout 约束。鉴权依赖不可用时明确返回服务不可用；可降级缓存应快速跳过。
- 登录限流使用可信代理解析后的客户端地址，不能无条件信任 `X-Forwarded-For`。
- 口令只用 cnetmod.security 提供的安全哈希能力，禁止应用层自写密码学。
- 权限失败、令牌失败、依赖失败和数据库失败必须映射为不同的注册错误码，不能一律返回 401/403。

## 9. 协程、多事件循环与并发

- 使用 `cnetmod::task<T>` + `co_await`；事件循环线程上禁止同步阻塞。
- CPU 密集任务通过 executor/offload；完成后必须恢复到发起请求所属事件循环，而不是固定控制循环。
- 请求 socket、SSE stream 和 request-scoped repository 只能在所属循环使用。
- 多事件循环资源遵循 cnetmod 的 sharded/owned 语义；不要凭“单例对象”推断底层连接池只有一份。
- 协程路径只准用 cnetmod 协程锁/信号量；禁止 `std::mutex`、`std::condition_variable`。
- 超时、取消、客户端断开、上游失败分别建模；看门狗关闭连接不等于自动取消所有上游工作。
- 连接池 `maximum_size` 是应用总量还是分片量必须以当前 cnetmod 文档和实现为准，并与
  `application.io_threads` 一起验证。

## 10. C++23 与模块规则

- 使用 C++23 Modules 与 `import std;`；禁止为标准库新增 `#include`。
- 标准库符号必须带 `std::` 前缀。
- 每个使用到的 cnetmod 模块必须直接 import；不得依赖传递 import 的偶然可见性。
- 统一使用 `std::expected` 和注册错误码；不可恢复的构建错误才抛异常交给 Application build 捕获。
- 唯一日志方式是 cnetmod logger；禁止 `printf`、`std::cout`、`std::println` 和 iostream 日志。
- 耗时或阻塞第三方 API 必须通过 cnetmod executor bridge 接入。
- 接口/实现职责清晰，抽象必须对应真实变化点；禁止为了模式而增加 resolver、factory、wrapper 层。

## 11. 配置、错误契约和命名

- 顶层配置由拥有它的 Application module 声明 typed options；未知键必须在构建期报带路径错误。
- 环境变量统一使用 `CNETBOOT_` 前缀；示例配置不得携带其它项目品牌。
- 服务实例名、组件名、错误码、权限码要稳定且表达语义。
- 新增错误码必须同步：enum、registry、`backend/contracts/error_codes.yaml` 和契约测试。
- 文件、类、函数、路由和表名使用具体业务词；禁止 `misc`、`utils2`、`common_manager` 等模糊命名。
- 不得在根目录或顶层模块列表中擅自增加目录；新增 bounded context 要同步 CMake、文档和门禁白名单。

## 12. 修改流程

AI 每次修改应按以下顺序工作：

1. 读取本文件与最新 cnetmod `AGENTS.md`/相关 skill。
2. 检查 `.codegraph/`；存在时先用 CodeGraph 定位符号和调用链，再使用 `rg` 补充精确搜索。
3. 检查工作区状态，保留用户已有改动，不覆盖不相关文件。
4. 先确认 cnetmod 是否已有能力，再确定修改属于 framework、业务模块还是 bootstrap。
5. 同步修改接口、实现、CMake、配置、迁移、契约、测试和文档，不能只修一层。
6. 运行格式/静态门禁、完整构建和测试；构建失败不得提交。
7. 提交前运行 `git diff --check`，确认没有生成物、密钥、绝对个人路径或垂直业务泄漏。

## 13. 强制门禁与验证

`cnetboot_framework_layering_boundaries` 必须持续检查并拒绝：

- 基础模板出现未登记的业务模块；
- 垂直业务词进入生产源码、配置、迁移或文档；
- DTO 放进 controller；
- ORM record 放进 mapper；
- 模糊 Service 名称；
- AI provider 类型越过 `ai/service/model` 边界；
- system/chat 的 DTO 目录或 AI model 边界被删除。

最低验收命令（路径按实际 cnetmod 安装调整）：

```bash
cmake -S backend -B backend/build-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH=/path/to/cnetmod/install \
  -Dyaml_cpp_modules_DIR=/path/to/yaml_cpp_modules/lib/cmake/yaml_cpp_modules \
  -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build-clang -j2
ctest --test-dir backend/build-clang --output-on-failure -j2
git diff --check
```

涉及 MySQL、Redis、租户/数据权限或故障降级时，不能只跑纯内存测试：必须启用对应真实服务测试，
覆盖正常、不可达、超时、并发、撤销、逻辑删除、跨租户和上下级边界。测试因环境缺失而 Skip 必须
在交付说明中明确列出，不能宣称其已经通过。

## 14. 提交前清单

- [ ] AI 已完整读取最新 cnetmod `AGENTS.md` 和相关 skill。
- [ ] 没有重造 cnetmod 已有能力。
- [ ] 没有垂直业务语义污染 cnetBoot。
- [ ] DTO、domain record、mapper、service、controller 位置正确。
- [ ] 没有手写原始 JSON、硬编码 secret、数据库时区函数或字符串解析数据库错误。
- [ ] AI 能力由组合根显式开启，Chat 只依赖应用层 LLM 契约。
- [ ] SSE 使用 `with_sse()`，协程恢复到请求所属事件循环。
- [ ] 配置、错误契约、迁移、CMake、测试和文档同步。
- [ ] Clang/libc++ 完整构建成功。
- [ ] CTest、架构门禁和真实依赖测试按改动范围通过。
- [ ] `git diff --check` 通过，工作区只包含本任务改动。
