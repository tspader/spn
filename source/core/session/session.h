#ifndef SPN_SESSION_SESSION_H
#define SPN_SESSION_SESSION_H

#include "sp.h"
#include "spn/core.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/types.h"
#include "target/types.h"

spn_err_t spn_session_init(spn_session_t* session, spn_ctx_t* ctx, sp_mem_t mem, spn_project_t* project, spn_session_config_t config);
spn_err_t spn_session_apply_options(spn_session_t* session, bool* reresolve);
void spn_session_export_toolchain_env(spn_session_t* session);
spn_err_t spn_session_validate_flags(spn_session_t* session);
sp_opt_spn_linkage_t spn_session_config_kind(spn_session_t* session, sp_str_t pkg_name);
spn_pkg_id_t spn_session_root_pkg(spn_session_t* session);
spn_pkg_unit_t* spn_session_find_pkg_unit_by_id(spn_session_t* session, spn_pkg_unit_id_t id);
spn_pkg_unit_t* spn_session_find_pkg_unit(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t pkg);
spn_pkg_unit_t* spn_session_find_dep(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t qualified, spn_dep_kind_t kind);
spn_target_unit_t* spn_session_find_target_in_pkg(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t name, spn_target_kind_t kind);
spn_target_unit_t* spn_session_get_target_unit(spn_session_t* session, spn_target_unit_id_t id);

#endif
