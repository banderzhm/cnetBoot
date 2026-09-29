/**
 * @brief 组合根入口：用 cnetmod application_builder 装配 Nexus 的各个应用模块。
 */
export module cnetnexus.bootstrap.application;

import std;

export namespace cnetnexus::bootstrap {

/**
 * @brief Builds and runs the cnetmod administration application.
 * @return 进程退出码：0 正常退出；1 构建或运行失败（原因已写入日志）
 */
[[nodiscard]] auto run(std::string configuration_file) -> int;

} // namespace cnetnexus::bootstrap
