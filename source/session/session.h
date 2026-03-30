#ifndef SPN_SESSION_SESSION_H
#define SPN_SESSION_SESSION_H

#include "err.h"
#include "session/types.h"

void spn_init_pkg_unit_for_session(spn_session_t* session, spn_pkg_unit_t* unit, spn_pkg_t* pkg, spn_pkg_kind_t kind);

void spn_session_init(spn_session_t* s, spn_pkg_t* pkg, spn_profile_t* profile, sp_str_t dir);
spn_err_t spn_session_compile_pkg(spn_session_t* s, spn_pkg_unit_t* ctx);
spn_pkg_unit_t* spn_session_find_pkg(spn_session_t* s, sp_str_t name);
spn_pkg_unit_t* spn_session_find_root(spn_session_t* s);
void spn_session_set_filter(spn_session_t* s, spn_target_filter_t filter);
spn_pkg_unit_t* spn_session_find_pkg_or_assert(spn_session_t* s, sp_str_t name);

#endif
