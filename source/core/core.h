#ifndef SPN_CORE_CORE_H
#define SPN_CORE_CORE_H

#include "core/types.h"

sp_str_t spn_tree_root(spn_tree_roots_t roots, spn_tree_t tree);
sp_str_t spn_tree_path_resolve(sp_mem_t mem, spn_tree_roots_t roots, spn_tree_path_t entry);

void spn_wake_ring(spn_wake_t* wake);
void spn_wake_pulse(spn_wake_t* wake);
void spn_wake_rearm(spn_wake_t* wake);

#endif
