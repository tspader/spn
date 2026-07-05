#ifndef SPN_PKG_ID_H
#define SPN_PKG_ID_H

#include "sp.h"

#include "intern/types.h"
#include "pkg/types.h"

spn_pkg_id_t    spn_pkg_id(sp_intern_t* intern, sp_str_t qualified);

sp_intern_str_t spn_pkg_name_to_qualified(spn_pkg_name_t name);
spn_pkg_name_t  spn_pkg_name_from_qualified(sp_str_t qualified);
sp_intern_str_t spn_pkg_canonicalize_name(sp_str_t);
sp_intern_str_t spn_pkg_canonicalize_pair(sp_str_t namespace, sp_str_t name);

#endif
