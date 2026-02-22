#ifndef SPN_AUTOCONF_H
#define SPN_AUTOCONF_H

#include "sp.h"
#include "spn.h"

struct spn_autoconf {
  spn_build_ctx_t* build;
  sp_da(sp_str_t) flags;
};

#endif
