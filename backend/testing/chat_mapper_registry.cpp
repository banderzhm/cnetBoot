import std;
import cnetmod.orm;
import cnetnexus.chat.mapper.chat_mapper;
import cnetnexus.chat.domain.chat_record;

namespace {

auto occurrences(std::string_view text, std::string_view value) -> std::size_t {
  std::size_t count{};
  for (auto position = text.find(value); position != std::string_view::npos;
       position = text.find(value, position + value.size()))
    ++count;
  return count;
}

} // namespace

auto main(int argc, char **argv) -> int {
  if (argc != 2)
    return 1;
  cnetmod::orm::mapper_registry registry;
  if (!registry.load_file(std::filesystem::path{argv[1]} / "ChatMapper.xml"))
    return 2;
  if (!cnetnexus::chat::validate_chat_mapper_registry(registry))
    return 3;
  if (registry.find_statement("ChatMapper.insertSession") != nullptr ||
      registry.find_statement("ChatMapper.insertMessage") != nullptr)
    return 4;

  cnetmod::orm::logical_delete_config deleted_at;
  deleted_at.field_name = "deleted_at";
  deleted_at.mode = cnetmod::orm::logical_delete_mode::nullable_datetime;
  cnetmod::orm::automatic_interceptor_options options;
  options.logical_delete_policy = deleted_at;
  auto sessions = cnetmod::orm::make_automatic_interceptor_chain<
      cnetnexus::chat::chat_session_record>(options);
  auto messages = cnetmod::orm::make_automatic_interceptor_chain<
      cnetnexus::chat::chat_message_record>(options);
  if (!sessions || !messages)
    return 5;
  auto session_query =
      (*sessions)->apply(cnetmod::orm::sql_operation::query,
                         {"SELECT * FROM `chat_session` WHERE `id` = {}",
                          {cnetmod::orm::param_value::from_int(1)}});
  auto message_query = (*messages)->apply(
      cnetmod::orm::sql_operation::query,
      {"SELECT * FROM `chat_message` WHERE `session_id` = {}",
       {cnetmod::orm::param_value::from_int(1)}});
  if (!session_query || !message_query ||
      !session_query->sql.contains("`deleted_at` IS NULL") ||
      !message_query->sql.contains("`deleted_at` IS NULL"))
    return 6;

  const auto *statement = registry.find_statement("ChatMapper.selectSession");
  const auto *fragments = registry.get_fragments("ChatMapper");
  if (!statement || !fragments)
    return 7;
  cnetmod::orm::param_context parameters;
  parameters.set("session_id", std::uint64_t{42});
  parameters.set("algorithm", std::string{});
  cnetmod::orm::dynamic_sql_processor processor{cnetmod::orm::format_options{}};
  auto rendered = processor.process(*statement, parameters, *fragments);
  auto intercepted =
      (*sessions)->apply(cnetmod::orm::sql_operation::query,
                         {std::move(rendered.sql), std::move(rendered.params)});
  if (!intercepted || occurrences(intercepted->sql, "deleted_at` IS NULL") != 1)
    return 8;

  cnetmod::orm::data_permission_interceptor<
      cnetnexus::chat::chat_session_record>
      owner_filter{cnetmod::orm::data_permission_scope{
          .include_owner = true,
          .owner_id = 42,
      }};
  auto owner_rendered = processor.process(*statement, parameters, *fragments);
  const auto owner_parameter_count = owner_rendered.params.size();
  auto owner_guarded = owner_filter.apply(
      cnetmod::orm::sql_operation::query,
      {std::move(owner_rendered.sql), std::move(owner_rendered.params)});
  if (!owner_guarded || !owner_guarded->sql.contains("s.owner_id =") ||
      owner_guarded->parameters.size() != owner_parameter_count + 1)
    return 9;
  return 0;
}
