#ifndef SPN_AUTOCONF_H
#define SPN_AUTOCONF_H

#include "sp.h"
#include "spn.h"

struct spn_autoconf {
  sp_mem_t mem;
  spn_t* build;
  sp_da(sp_str_t) flags;
};

s32 spn_autoconf(spn_t* build);
spn_autoconf_t* spn_autoconf_new(spn_t* build);
s32 spn_autoconf_run(spn_autoconf_t* autoconf);
void spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag);

#endif
