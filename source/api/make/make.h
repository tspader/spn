#ifndef SPN_MAKE_H
#define SPN_MAKE_H

#include "sp.h"
#include "spn.h"

struct spn_make {
  sp_mem_t mem;
  spn_t* build;
  sp_str_t target;
};

s32 spn_make(spn_t* build);
spn_make_t* spn_make_new(spn_t* build);
void spn_make_add_target(spn_make_t* make, const c8* target);
s32 spn_make_run(spn_make_t* make);

#endif
