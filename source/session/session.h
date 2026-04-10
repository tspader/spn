#ifndef SPN_SESSION_SESSION_H
#define SPN_SESSION_SESSION_H

#include "error/types.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/types.h"

spn_err_t compile_package(spn_session_t* s, spn_pkg_unit_t* ctx);
spn_pkg_unit_t* spn_session_add_pkg(spn_session_t* s, spn_loaded_pkg_t* loaded);
spn_target_unit_t* spn_session_add_target(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info);
spn_pkg_unit_t* spn_session_find_root(spn_session_t* s);
spn_pkg_unit_t* spn_session_find_pkg_by_qualified(spn_session_t* s, sp_str_t qualified);
spn_target_unit_t* spn_session_find_target_in_pkg(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t name);


#endif
