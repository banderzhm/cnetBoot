/**
 * @brief LLM 文本补全契约（读解与对话两个用例共用）。
 *
 * 供应商无关：这里只有消息 / 请求 / 结果三个纯数据类型与增量回调，具体实现（OpenAI 兼容
 * 网关）由组合根注入，不外泄任何适配器或 HTTP 类型。
 */
export module cnetboot.ai.domain.llm_contract;

import std;
import cnetmod.coro.task;

export namespace cnetboot::ai {

/** @brief 一条对话消息。 */
struct llm_message
{
    std::string role;    ///< system / user / assistant
    std::string content; ///< 文本内容
};

/** @brief 一次文本补全请求。 */
struct llm_request
{
    std::vector<llm_message> messages; ///< 对话消息；system 必须排在 user 之前
    std::string provider;              ///< 供应商名；空表示由实现取配置默认值（向后兼容）
    std::string model;                 ///< 模型名；空表示由实现取配置默认值
    double temperature{0.2};           ///< 采样温度；解读类任务默认低温
    int max_tokens{1024};              ///< 输出上限
    std::string reasoning_effort;      ///< 推理强度；空表示不发送
};

/** @brief 一次补全的结果：文本 + 计量。 */
struct llm_completion
{
    std::string content; ///< 模型输出全文
    int tokens_in{};     ///< 输入 token；上游未返回时为 0
    int tokens_out{};    ///< 输出 token；上游未返回时为 0
};

/** @brief 流式增量回调：返回 false 表示下游不再接收（如客户端断开），实现应尽快中断上游流。 */
using llm_delta_sink = std::function<cnetmod::task<bool>(std::string_view)>;

} // namespace cnetboot::ai
