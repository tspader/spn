#include "toolchain/select.h"

#include "toolchain/catalog.h"
#include "triple/triple.h"

sp_str_t spn_toolchain_script_default(void) {
  return sp_str_lit("zig");
}

spn_triple_t spn_toolchain_script_target(void) {
  return (spn_triple_t) { .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI, .abi = SPN_ABI_NONE };
}

bool spn_toolchain_supports(spn_toolchain_t* toolchain, spn_triple_t target, spn_triple_t host) {
  if (sp_da_empty(toolchain->targets)) {
    return spn_triple_match(target, host) && spn_triple_match(host, target);
  }

  sp_da_for(toolchain->targets, it) {
    if (spn_triple_match(toolchain->targets[it], target)) return true;
  }

  return false;
}

spn_toolchain_select_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_mem_t mem, spn_toolchain_selection_t* out) {
  *out = (spn_toolchain_selection_t) sp_zero;
  out->required = sp_da_new(mem, spn_toolchain_t*);

  struct {
    sp_str_t name;
    spn_triple_t target;
    spn_toolchain_role_t role;
    spn_toolchain_t** slot;
  } roles [] = {
    { query.build, query.target, SPN_TOOLCHAIN_ROLE_BUILD, &out->build },
    { query.script, spn_toolchain_script_target(), SPN_TOOLCHAIN_ROLE_SCRIPT, &out->script },
  };

  sp_carr_for(roles, it) {
    spn_toolchain_t* toolchain = spn_toolchain_catalog_get(catalog, roles[it].name);
    if (!toolchain) {
      return (spn_toolchain_select_err_t) {
        .status = SPN_TOOLCHAIN_SELECT_ERR_UNKNOWN,
        .role = roles[it].role,
        .name = roles[it].name,
      };
    }

    if (!spn_toolchain_supports(toolchain, roles[it].target, query.host)) {
      return (spn_toolchain_select_err_t) {
        .status = SPN_TOOLCHAIN_SELECT_ERR_TARGET,
        .role = roles[it].role,
        .name = roles[it].name,
        .target = roles[it].target,
      };
    }

    *roles[it].slot = toolchain;

    bool seen = false;
    sp_da_for(out->required, r) {
      if (out->required[r] == toolchain) seen = true;
    }
    if (!seen) sp_da_push(out->required, toolchain);
  }

  return (spn_toolchain_select_err_t) { .status = SPN_TOOLCHAIN_SELECT_OK };
}
