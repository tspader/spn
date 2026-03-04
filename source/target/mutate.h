#ifndef SPN_TARGET_MUTATE_H
#define SPN_TARGET_MUTATE_H

#include "target/types.h"

void spn_target_add_source(spn_target_t* target, const c8* source);
void spn_target_add_source_ex(spn_target_t* target, sp_str_t source);
void spn_target_add_include(spn_target_t* target, const c8* include);
void spn_target_add_include_ex(spn_target_t* target, sp_str_t include);
void spn_target_add_define(spn_target_t* target, const c8* define);
void spn_target_add_define_ex(spn_target_t* target, sp_str_t define);
void spn_target_set_visibility(spn_target_t* target, spn_visibility_t visibility);
void spn_linkage_set_add(spn_linkage_set_t* set, spn_linkage_t kind);
bool spn_linkage_set_has(spn_linkage_set_t set, spn_linkage_t kind);
spn_linkage_t spn_linkage_set_default(spn_linkage_set_t set);


#endif
