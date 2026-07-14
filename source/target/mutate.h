#ifndef SPN_TARGET_MUTATE_H
#define SPN_TARGET_MUTATE_H

#include "forward/types.h"
#include "target/types.h"

static inline void spn_target_info_init(sp_mem_t mem, spn_target_info_t* target) {
  if (!target->source)  sp_da_init(mem, target->source);
  if (!target->headers) sp_da_init(mem, target->headers);
  if (!target->include) sp_da_init(mem, target->include);
  if (!target->define)  sp_da_init(mem, target->define);
  if (!target->flags)   sp_da_init(mem, target->flags);
  if (!target->system_deps) sp_da_init(mem, target->system_deps);
  if (!target->deps)    sp_da_init(mem, target->deps);
  if (!target->embed)   sp_da_init(mem, target->embed);
  if (!target->macos.frameworks) sp_da_init(mem, target->macos.frameworks);
  if (!target->gated.source)      sp_da_init(mem, target->gated.source);
  if (!target->gated.define)      sp_da_init(mem, target->gated.define);
  if (!target->gated.flags)       sp_da_init(mem, target->gated.flags);
  if (!target->gated.system_deps) sp_da_init(mem, target->gated.system_deps);
  if (!target->gated.deps)        sp_da_init(mem, target->gated.deps);
}
void spn_target_add_source_ex(spn_target_info_t* target, sp_str_t source);
void spn_target_add_header_ex(spn_target_info_t* target, sp_str_t header);
void spn_target_add_include_ex(spn_target_info_t* target, sp_str_t include);
void spn_target_add_define_ex(spn_target_info_t* target, sp_str_t define);
void spn_target_add_flag_ex(spn_target_info_t* target, sp_str_t flag);
void spn_target_add_dep(spn_target_info_t* target, const c8* dep);
void spn_target_add_dep_ex(spn_target_info_t* target, sp_str_t dep);
void spn_linkage_set_add(spn_linkage_set_t* set, spn_linkage_t kind);
bool spn_linkage_set_has(spn_linkage_set_t set, spn_linkage_t kind);
spn_linkage_t spn_linkage_set_default(spn_linkage_set_t set);


#endif
