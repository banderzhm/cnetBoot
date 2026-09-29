module;

#include <cnetmod/orm.hpp>

export module cnetnexus.system.domain.sys_menu;

import std;
import cnetmod.orm;

export namespace cnetnexus::system {

/**
 * @brief Route, action permission, or menu node visible to a role.
 */
struct sys_menu {
  std::int64_t menu_id{};
  std::int64_t parent_id{};
  std::string menu_name;
  std::string path;
  std::string component;
  std::string permission;
  std::string menu_type;
  std::string icon;
  std::int32_t order_num{};
  int visible{};
  int status{};
};

} // namespace cnetnexus::system

CNETMOD_MODEL(cnetnexus::system::sys_menu, "sys_menu",
              CNETMOD_FIELD(menu_id, "menu_id", bigint, PK | AUTO_INC),
              CNETMOD_FIELD(parent_id, "parent_id", bigint),
              CNETMOD_FIELD(menu_name, "menu_name", varchar),
              CNETMOD_FIELD(path, "path", varchar),
              CNETMOD_FIELD(component, "component", varchar),
              CNETMOD_FIELD(permission, "permission", varchar),
              CNETMOD_FIELD(menu_type, "menu_type", varchar),
              CNETMOD_FIELD(icon, "icon", varchar),
              CNETMOD_FIELD(order_num, "order_num", int_),
              CNETMOD_FIELD(visible, "visible", tinyint),
              CNETMOD_FIELD(status, "status", tinyint))
