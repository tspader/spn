#include "spn/host.h"

#include "op/build/build.h"
#include "pkg/types.h"
#include "session/session.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

static spn_target_unit_t* unwrap(spn_target_t* target) {
  return sp_ptr_cast(spn_target_unit_t*, target);
}

static spn_target_t* wrap(spn_target_unit_t* unit) {
  return sp_ptr_cast(spn_target_t*, unit);
}

u32 spn_session_num_targets(spn_session_t* session) {
  u32 count = 0;
  sp_da_for(session->plans, it) {
    count += sp_da_size(session->plans[it].roots);
  }
  return count;
}

spn_target_t* spn_session_target_at(spn_session_t* session, u32 index) {
  sp_da_for(session->plans, it) {
    spn_build_plan_t* plan = &session->plans[it];
    u32 num_roots = sp_da_size(plan->roots);
    if (index < num_roots) {
      return wrap(spn_session_get_target_unit(session, plan->roots[index]));
    }
    index -= num_roots;
  }
  return SP_NULLPTR;
}

spn_target_t* spn_session_script_root(spn_session_t* session) {
  u32 num_targets = spn_session_num_targets(session);
  sp_for(it, num_targets) {
    spn_target_t* target = spn_session_target_at(session, it);
    if (spn_target_kind(target) == SPN_TARGET_SCRIPT) {
      return target;
    }
  }
  return SP_NULLPTR;
}

sp_str_t spn_target_name(spn_target_t* target) {
  return unwrap(target)->info->name;
}

spn_target_kind_t spn_target_kind(spn_target_t* target) {
  return unwrap(target)->info->kind;
}

spn_pkg_info_t* spn_target_pkg(spn_target_t* target) {
  return unwrap(target)->pkg->info;
}

sp_str_t spn_target_staged_path(sp_mem_t mem, spn_target_t* target) {
  return spn_target_unit_staged_path(mem, unwrap(target));
}
