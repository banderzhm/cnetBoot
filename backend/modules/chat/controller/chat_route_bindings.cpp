module;

#include <cnetmod/json_codec.hpp>

module cnetboot.chat.controller.chat_route_context;

import std;
import cnetmod.protocol.http;
import cnetboot.framework.security.authorization;
import cnetboot.framework.web.rate_limit;
import cnetboot.framework.web.route_handler;

namespace cnetboot::chat::detail {

void declare_chat_handlers(cnetmod::http::router &routes,
                           const chat_route_context &services) {
  using handler = cnetboot::framework::web::route_handler<chat_route_context>;

  routes.post("/api/v1/chat/sessions", handler{services, handle_session_create},
              cnetboot::security::requires_permission("chat:session:create"));
  routes.get("/api/v1/chat/sessions", handler{services, handle_session_list},
             cnetboot::security::requires_permission("chat:session:list"));
  routes.get("/api/v1/chat/sessions/:id", handler{services, handle_session_read},
             cnetboot::security::requires_permission("chat:session:list"));
  routes.patch("/api/v1/chat/sessions/:id",
               handler{services, handle_session_update},
               cnetboot::security::requires_permission("chat:session:update"));
  routes.del("/api/v1/chat/sessions/:id", handler{services, handle_session_delete},
             cnetboot::security::requires_permission("chat:session:delete"));
  routes.get("/api/v1/chat/sessions/:id/messages",
             handler{services, handle_message_list},
             cnetboot::security::requires_permission("chat:session:list"));
  routes.post("/api/v1/chat/sessions/:id/messages",
              handler{services, handle_message_send},
              cnetboot::framework::web::with_rate_limit(
                  cnetboot::security::requires_permission("chat:message:send"),
                  cnetboot::framework::web::rate_limit_policy::ai));
}

} // namespace cnetboot::chat::detail
