# 分层约定

## 业务模块的分层

`modules/<module>/` 只允许这五种目录：

| 目录 | 职责 | 允许依赖 |
|---|---|---|
| `controller/` | HTTP 适配：解析请求、声明路由、写响应 | service、dto、非持久化 domain 命令/结果 |
| `service/` | 用例编排、事务边界、外部依赖翻译 | domain、mapper、dto 输出契约 |
| `domain/` | 领域模型、ORM 实体、强类型查询投影与不变量，不含 I/O | std、cnetmod ORM 元数据 |
| `mapper/` | 持久化接口与实现：XML 语句、仓储操作和查询映射 | domain |
| `dto/` | 传输对象与序列化映射 | domain |

控制器不碰会话，service 不写 SQL，domain 不做 I/O。
`dto/` 与 `controller/` 是同级目录：controller 只负责 HTTP 适配并调用 DTO 投影，
任何 `*.dto.*` 模块、`*dto.cpp[m]` 文件都禁止放进 controller；DTO 也禁止反向
import controller。

请求级数据权限仓储由 `bootstrap/` 组合根创建：组合根通过
`repository_factory::for_request()` 固化当前请求的数据权限，再把只暴露 Service 的
解析回调注入 Controller。Service Factory 只接收已经绑定好的仓储，不依赖 HTTP；
Controller 不得 import Mapper/ORM，不得持有 repository factory 或 managed repository；
Mapper 也不得反向 import Service。

Controller 只能承担传输适配。跨两个以上业务能力的编排必须形成有业务语义的 Service：
批量排盘、逐条失败隔离与存档由 `chart_generation_service` 承担；排盘、运限投影与
解读材料生成由 `chart_reading_service` 承担。Controller 不得自己串联多个 Service
完成一个业务用例。

Service 名称必须表达业务用例或领域能力。允许 `chart_generation_service`、
`authorization_service` 这类名称；禁止 `batch_service`、`common_service`、
`data_service`、`base_service`、`manager_service`、`helper_service` 等只描述传输形态、
技术机制或泛化职责的名称。

## 外部依赖的适配层放哪

上游依赖（ZhouYiLab、cnetmod 的模型调用）需要一层 anti-corruption layer：
把外部类型、外部异常语义翻译成本项目的领域模型与错误码。

**结论：放 `service/<upstream>/`，不单独占一层。**

理由：

1. 它履行的就是 service 的职责——翻译与边界收口，不做业务编排、不碰 HTTP、不做持久化。
   单独叫 `client/` 会让人误以为它是"第五个业务层"，而它其实只服务于 service。
2. 目录名用**上游名**（`zhouyi`/`xuanxue`），一眼能看出这个目录装的是哪家的适配。
3. 上游类型不得越过这层边界：领域模型里只出现本项目的类型。

## 当前的外部依赖适配层

| 目录 | 上游 | 约束 |
|---|---|---|
| `modules/chart/service/xuanxue/` | ZhouYiLab（`import ZhouYi.*`） | **全仓库唯一**允许 import ZhouYi 的目录 |
| `modules/reading/service/model/` | cnetmod 的模型调用（`cnetmod.ai` / `chat_model_*`） | **全仓库唯一**允许 import 模型上游的目录 |

两者都放在 `service/` 之下，符合"适配层只服务于 service"的定位。

### 为什么第二个叫 `model` 而不是厂商名

按第一条的约定本应叫 `service/cnetmod/`，但 `cnetmod` 是整个框架的底座——
业务模块里有 15 个文件 import `cnetmod.application`、45 个 import
`cnetmod.protocol.http`，用厂商名命名会失去区分度。

这个目录真正特殊的地方是它封装了**一个能力**：模型调用——每个供应商显式注册一个
cnetmod 组合模型，Nexus 明确开启多 key 轮询与失败切换，以及 `composed_llm_service` 的
目标解析与协议翻译。因此按能力命名为
`model`。命名规则可归纳为：**上游是窄能力边界时按能力命名，是单一外部产品时按产品命名**。

## 边界怎么守

边界不是靠文档约定，而是靠一个**可执行的用例**
（`testing/framework_layering_boundaries.cpp`，已注册为 CTest）：
它扫描源码树，断言

1. `import ZhouYi` 只出现在 `modules/chart/service/xuanxue/`；
2. `cnetmod.ai` / `chat_model_*`（含 `chat_model_composition`）只出现在
   `modules/reading/service/model/`；
3. `modules/<module>/` 下的目录名只能是 `controller/service/domain/mapper/dto`；
4. 旧的 `modules/*/client` 分层不再存在。
5. `cnetnexus.framework.web.*` 只允许由 `controller/` 导入；分页等跨层契约
   必须使用中立的 `cnetnexus.framework.pagination`。
6. DTO 的文件路径、模块名和依赖方向一致：只能位于 `modules/<module>/dto/`，
   模块名必须属于 `cnetnexus.<module>.dto.*`，且不得依赖 controller。
7. 所有业务源码的物理路径必须与具名模块一致：
   `modules/<module>/<layer>/...` 只能声明
   `cnetnexus.<module>.<layer>.*`，防止任何层的代码被放错目录。
8. 非 controller 不得 import controller；domain 不得 import service/mapper/dto；
   dto 不得 import service/mapper，mapper 不得 import dto。数据库实体和强类型查询投影
   归 domain，不能混入 mapper 或对外 DTO。`cnetmod.protocol.http` 和
   `cnetnexus.framework.web.*` 也只能出现在 controller。
9. `CNETMOD_MODEL(...)` 与 `CNETMOD_PROJECTION(...)` 只能声明在 domain；mapper
   只消费这些类型并完成数据库操作。
10. Spring 风格调用链固定为 Controller → Service → Mapper → Domain：Controller
    禁止 import mapper/ORM 或持有仓储句柄，Mapper 禁止 import Service。请求级仓储
    组装统一封装在 bootstrap 组合根，不能散落在路由处理器；Service Factory 不得
    反向依赖 HTTP。
11. Service 必须使用明确的业务能力命名；门禁拒绝 `batch_service` 等泛化或技术形态名。

它自身位于 `testing/`，扫描时跳过该目录（用例源码里含有被检查的模式字符串，
扫到自己会误报）。

验证该守卫真的有效：向 `modules/chart/domain/bazi.cppm` 追加一行
`import ZhouYi.BaZi;`，用例立刻失败并打印违规文件路径；移除后重新通过。
三类违规（领域层 import 上游、controller import 模型上游、新增非法分层目录）
均已逐一验证会被抓住。

路由访问策略另由 `testing/framework_route_contract.cpp` 直接读取 cnetmod
`router::endpoints()` 的元数据校验。该用例覆盖认证、目录、六条 reading、案例/标签与
chat 共 28 条核心端点，固定方法、路径和 anonymous / optional authentication /
required permissions 语义，并拒绝重复的“方法 + 路径”。它不是第二份运行时白名单，
不会参与请求分发。

## 与 CMake 的关系

`chart` 目标带 `ZHOYI` 标记，会编译上游模块集。适配层留在这个目标内，
是为了避免把"编译第三方模块集"的负担加到 `framework/`（框架层只做基础设施）。
