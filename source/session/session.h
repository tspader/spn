#ifndef SPN_SESSION_SESSION_H
#define SPN_SESSION_SESSION_H

#include "err.h"
#include "pkg/types.h"
#include "session/types.h"

spn_err_t compile_package(spn_session_t* s, spn_pkg_unit_t* ctx);
spn_pkg_unit_t* spn_session_find_pkg(spn_session_t* s, sp_str_t name);
spn_pkg_unit_t* spn_session_find_root(spn_session_t* s);
spn_pkg_unit_t* spn_session_find_pkg_or_assert(spn_session_t* s, sp_str_t name);
spn_pkg_unit_t* spn_session_add_pkg(spn_session_t* s, spn_loaded_pkg_t* pkg);

#endif
