#ifndef SPN_TARGET_CLOSURE_H
#define SPN_TARGET_CLOSURE_H

#include "sp.h"

#include "forward/types.h"
#include "pkg/types.h"
#include "target/types.h"

sp_da(spn_closure_entry_t) spn_target_link_closure(sp_mem_t mem, spn_target_unit_t* root);
sp_da(spn_link_lib_t) spn_closure_link_libs(sp_mem_t mem, sp_da(spn_closure_entry_t) closure);
sp_da(spn_target_unit_t*) spn_target_runtime_libs(sp_mem_t mem, spn_target_unit_t* root);
bool spn_dep_kind_applies(spn_dep_kind_t dep, spn_target_kind_t target);

#endif
