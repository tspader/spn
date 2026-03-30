#ifndef SPN_PKG_ID_H
#define SPN_PKG_ID_H

#include "sp.h"

#include "pkg/types.h"

sp_str_t     spn_pkg_id_to_qualified_name(spn_pkg_id_t id);
spn_pkg_id_t spn_qualified_name_to_pkg_id(sp_str_t qualified);
sp_str_t     spn_pkg_canonicalize_name(sp_str_t);
sp_str_t     spn_pkg_canonicalize_pair(sp_str_t namespace, sp_str_t name);

#endif
