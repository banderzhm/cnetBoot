# cnetBoot

cnetBoot 是基于 cnetmod 的 C++23 后端基础脚手架。它只提供可复用的应用底座：

- RBAC：用户、角色、菜单权限、部门与数据范围；
- JWT + Redis 会话：登录、刷新、撤销、空闲续期与登录防爆破；
- 审计日志、统一错误契约、分页、限流、SSE；
- 显式启用的多供应商、多模型、多 Key AI；
- 通用多轮 Chat，会话分类和参考上下文均不绑定垂直业务。

仓库刻意不包含任何行业算法、内容生产流程或行业实体。业务项目应把自己的 bounded
context 新增到 `backend/modules/<business>/`，不能反向污染 system、ai 或 chat。

## 目录

```text
backend/
  bootstrap/        Application 模块与唯一组合根
  framework/        错误、配置、安全、审计、Web 基础设施
  modules/
    system/         RBAC 与认证业务
    ai/             模型目录、供应商选择与 cnetmod 模型适配
    chat/           通用会话与消息业务
  migrations/       MySQL 迁移
  testing/          契约、分层和组件测试
```

业务模块采用 Spring 风格的同级分层：`controller/`、`dto/`、`service/`、`domain/`、
`mapper/`。DTO 不放进 controller，ORM 实体放在 domain，mapper 只承担持久化边界。

## 配置

复制并调整 `backend/config/application.yaml`，至少提供：

```bash
export CNETBOOT_DATABASE_PASSWORD='replace-me'
export CNETBOOT_JWT_SECRET='replace-with-at-least-32-characters'
```

`llm.providers` 为空时 AI 明确关闭，服务仍可启动。启用方式见
`backend/config/llm.providers.example.yaml`。模型能力由 `ai_module` 显式注册，不因
出现配置文件或路由而隐式开启。

## WSL / Clang 构建

```bash
cmake -S backend -B backend/build -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH=/path/to/cnetmod/install \
  -Dyaml_cpp_modules_DIR=/path/to/yaml_cpp_modules/lib/cmake/yaml_cpp_modules \
  -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j2
ctest --test-dir backend/build --output-on-failure
```

## 扩展规则

1. 新业务必须创建具名模块，不允许用 `batch_service`、`common_service` 等无业务含义名称。
2. Controller 只做协议适配；业务编排属于 Service；数据库实体与规则属于 Domain。
3. 优先使用 cnetmod Application、ORM、JWT、SSE、Redis 与 AI 能力，不在应用层重造基础设施。
4. 修改架构后必须运行 `cnetboot_framework_layering_boundaries`，它会阻止垂直业务词、
   错误目录和反分层命名进入基础模板。
