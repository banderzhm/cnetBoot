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
3rdparty/
  cnetmod/          固定版本的 cnetmod Git submodule
script/             Windows / WSL 的依赖预构建和 backend 构建入口
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

## 获取依赖

克隆时初始化 cnetmod submodule：

```bash
git clone --recurse-submodules --shallow-submodules <cnetBoot-repository-url>
# 已经克隆仓库时：
git submodule update --init --recursive --depth 1
```

cnetBoot 不读取工作区外的 cnetmod，也不使用 vcpkg。构建脚本从
`3rdparty/cnetmod` 构建 cnetmod 自带的第三方依赖并安装为本仓库私有 CMake package。
若 submodule 尚未完整初始化，3rdparty 脚本会先执行递归初始化。

## WSL / Clang 构建

```bash
./script/wsl_clang_3rdparty_build.sh
CNETBOOT_BUILD_TESTING=ON ./script/wsl_clang_build.sh
ctest --test-dir backend/build-clang --output-on-failure -j2
```

第一条命令必须先执行；它把 cnetmod 安装到 `3rdparty/wsl-clang`。backend 脚本只使用这个
安装包，缺失时会直接报错，不会隐式回退到其它 cnetmod。

## Windows / MSVC 构建

```powershell
.\script\windows_3rdparty_build.ps1
.\script\windows_build.ps1 -BuildTests
ctest --test-dir backend/build-windows -C Release --output-on-failure
```

Windows 安装目录为 `3rdparty/windows`。默认限制 cnetmod 编译并行度为 1，避免 C++ Modules
完整构建占用过多内存；可按机器容量传 `-Jobs` 调整。
WSL 默认并行度为 2，可用 `CNETBOOT_JOBS` 显式调整。

## 扩展规则

1. 新业务必须创建具名模块，不允许用 `batch_service`、`common_service` 等无业务含义名称。
2. Controller 只做协议适配；业务编排属于 Service；数据库实体与规则属于 Domain。
3. 优先使用 cnetmod Application、ORM、JWT、SSE、Redis 与 AI 能力，不在应用层重造基础设施。
4. 修改架构后必须运行 `cnetboot_framework_layering_boundaries`，它会阻止垂直业务词、
   错误目录和反分层命名进入基础模板。
