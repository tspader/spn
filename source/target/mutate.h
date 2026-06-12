#ifndef SPN_TARGET_MUTATE_H
#define SPN_TARGET_MUTATE_H

#include "forward/types.h"
#include "target/types.h"

void spn_target_add_source_ex(spn_target_info_t* target, sp_str_t source);
void spn_target_add_header_ex(spn_target_info_t* target, sp_str_t header);
void spn_target_add_include_ex(spn_target_info_t* target, sp_str_t include);
void spn_target_add_define_ex(spn_target_info_t* target, sp_str_t define);
void spn_target_add_dep(spn_target_info_t* target, const c8* dep);
void spn_target_add_dep_ex(spn_target_info_t* target, sp_str_t dep);
void spn_linkage_set_add(spn_linkage_set_t* set, spn_linkage_t kind);
bool spn_linkage_set_has(spn_linkage_set_t set, spn_linkage_t kind);
spn_linkage_t spn_linkage_set_default(spn_linkage_set_t set);


#endif
