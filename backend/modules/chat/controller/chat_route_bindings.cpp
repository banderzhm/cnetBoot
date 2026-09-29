module;

#include <cnetmod/json_codec.hpp>

module cnetnexus.chat.controller.chat_route_context;

import std;
import cnetmod.protocol.http;
import cnetnexus.framework.security.authorization;
import cnetnexus.framework.web.route_handler;

namespace cnetnexus::chat::detail {

void declare_chat_handlers(cnetmod::http::router &routes,
                           const chat_route_context &services) {
  using handler = cnetnexus::framework::web::route_handler<chat_route_context>;

  routes.post("/api/v1/chat/sessions", handler{services, handle_session_create},
              cnetnexus::security::requires_permission("chat:session:create"));
  routes.get("/api/v1/chat/sessions", handler{services, handle_session_list},
             cnetnexus::security::requires_permission("chat:session:list"));
  routes.get("/api/v1/chat/sessions/:id", handler{services, handle_session_read},
             cnetnexus::security::requires_permission("chat:session:list"));
  routes.patch("/api/v1/chat/sessions/:id",
               handler{services, handle_session_update},
               cnetnexus::security::requires_permission("chat:session:update"));
  routes.del("/api/v1/chat/sessions/:id", handler{services, handle_session_delete},
             cnetnexus::security::requires_permission("chat:session:delete"));
  routes.get("/api/v1/chat/sessions/:id/messages",
             handler{services, handle_message_list},
             cnetnexus::security::requires_permission("chat:session:list"));
  routes.post("/api/v1/chat/sessions/:id/messages",
              handler{services, handle_message_send},
              cnetnexus::security::requires_permission("chat:message:send"));
}

} // namespace cnetnexus::chat::detail
