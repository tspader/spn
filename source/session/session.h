#ifndef SPN_SESSION_SESSION_H
#define SPN_SESSION_SESSION_H

#include "error/types.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/types.h"
#include "target/types.h"

spn_err_union_t spn_session_init(spn_session_t* session, spn_pkg_info_t* root, spn_app_config_t config);
spn_err_t spn_session_apply_options(spn_session_t* session);
sp_opt_spn_linkage_t spn_session_config_kind(spn_session_t* session, sp_str_t pkg_name);
sp_da(spn_pkg_dep_t) spn_session_pkg_deps(spn_session_t* session, spn_pkg_unit_t* pkg);
spn_pkg_unit_t* spn_session_add_pkg(spn_session_t* s, spn_pkg_id_t id, spn_loaded_pkg_t* loaded);
spn_target_unit_t* spn_session_add_target(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info);
spn_pkg_unit_t* spn_session_find_root(spn_session_t* s);
spn_pkg_unit_t* spn_session_find_pkg_by_id(spn_session_t* s, spn_pkg_id_t id);
spn_pkg_unit_t* spn_session_find_dep(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t qualified, spn_dep_kind_t kind);
spn_target_unit_t* spn_session_find_target_in_pkg(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t name);


#endif
