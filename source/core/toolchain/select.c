#include "toolchain/select.h"

#include "ctx/types.h"
#include "error/error.h"
#include "toolchain/catalog.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

typedef struct {
  spn_err_t err;
  spn_toolchain_target_t target;
} reach_t;

static bool usable(const spn_toolchain_info_t* toolchain) {
  return toolchain->support.kind != SPN_TOOLCHAIN_SUPPORT_NONE;
}

static const spn_toolchain_target_t* listed(const spn_toolchain_info_t* toolchain, spn_triple_t triple) {
  sp_da_for(toolchain->targets, it) {
    if (spn_triple_equal(toolchain->targets[it].triple, triple)) {
      return &toolchain->targets[it];
    }
  }
  return SP_NULLPTR;
}

static bool lists_any_abi(const spn_toolchain_info_t* toolchain, spn_triple_t target) {
  spn_triple_t pattern = { target.arch, target.os, SPN_ABI_NONE };
  sp_da_for(toolchain->targets, it) {
    if (spn_triple_match(pattern, toolchain->targets[it].triple)) {
      return true;
    }
  }
  return false;
}

static sp_da(spn_triple_t) triples(sp_mem_t mem, sp_da(spn_toolchain_target_t) targets) {
  sp_da(spn_triple_t) out = sp_da_new(mem, spn_triple_t);
  sp_da_for(targets, it) {
    sp_da_push(out, targets[it].triple);
  }
  return out;
}

static spn_triple_t with_abi(spn_triple_t target, spn_abi_t abi) {
  return (spn_triple_t) { target.arch, target.os, abi };
}

static spn_abi_list_t os_abis(spn_os_t os) {
  spn_abi_list_t list = sp_zero;
  const spn_abi_t* abis = SP_NULLPTR;
  list.count = spn_os_abis(os, &abis);
  sp_for(it, list.count) {
    list.items[it] = abis[it];
  }
  return list;
}

static spn_err_t reach(const spn_toolchain_info_t* toolchain, const spn_toolchain_catalog_t* catalog, spn_triple_t target) {
  if (listed(toolchain, target)) {
    return SPN_OK;
  }
  if (!spn_toolchain_driver_retargets(toolchain->driver)) {
    return SPN_ERR_TOOLCHAIN_TARGET;
  }
  switch (spn_sdk_kind(target)) {
    case SPN_SDK_NONE:    return SPN_OK;
    case SPN_SDK_SYSROOT: return spn_triple_equal(target, catalog->host) ? SPN_OK : SPN_ERR_TOOLCHAIN_SYSROOT;
    case SPN_SDK_MACOS:   return spn_sdk_find(catalog->sdks, target).kind ? SPN_OK : SPN_ERR_TOOLCHAIN_SDK_MACOS;
    case SPN_SDK_MSVC:    return spn_sdk_find(catalog->sdks, target).kind ? SPN_OK : SPN_ERR_TOOLCHAIN_SDK_MSVC;
  }
  sp_unreachable_return(SPN_ERR_TOOLCHAIN_TARGET);
}

static spn_toolchain_target_t matched(const spn_toolchain_info_t* toolchain, spn_triple_t triple) {
  const spn_toolchain_target_t* entry = listed(toolchain, triple);
  return entry ? *entry : (spn_toolchain_target_t) { .triple = triple };
}

static reach_t reach_first(const spn_toolchain_info_t* toolchain, const spn_toolchain_catalog_t* catalog, spn_triple_t target, spn_abi_list_t abis) {
  sp_for(it, abis.count) {
    spn_triple_t candidate = with_abi(target, abis.items[it]);
    if (reach(toolchain, catalog, candidate) == SPN_OK) {
      return (reach_t) { .target = matched(toolchain, candidate) };
    }
  }
  return (reach_t) { .err = reach(toolchain, catalog, with_abi(target, abis.items[0])) };
}

static bool satisfies(const spn_toolchain_info_t* toolchain, const spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_target_t* target) {
  if (!usable(toolchain)) {
    return false;
  }
  reach_t reached = reach_first(toolchain, catalog, query.target, query.abis);
  *target = reached.target;
  return reached.err == SPN_OK;
}

static sp_da(sp_str_t) satisfying(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_da(sp_str_t) names = sp_da_new(catalog->mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    spn_toolchain_target_t target = sp_zero;
    if (satisfies(entry, catalog, query, &target)) {
      sp_da_push(names, entry->name);
    }
  }
  return names;
}

static sp_da(sp_str_t) listing(spn_toolchain_catalog_t* catalog, spn_triple_t target) {
  sp_da(sp_str_t) names = sp_da_new(catalog->mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    if (usable(entry) && lists_any_abi(entry, target)) {
      sp_da_push(names, entry->name);
    }
  }
  return names;
}

static spn_err_t emit(spn_err_t kind, spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_da(sp_str_t) candidates, sp_da(spn_triple_t) targets) {
  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = kind,
    .toolchain = {
      .name = query.toolchain.name,
      .target = query.target,
      .host = catalog->host,
      .candidates = candidates,
      .targets = targets,
    },
  });
}

static spn_err_t emit_abi(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  spn_abi_list_t abis = os_abis(query.target.os);
  sp_da(spn_abi_t) candidates = sp_da_new(catalog->mem, spn_abi_t);
  sp_for(it, abis.count) {
    sp_da_push(candidates, abis.items[it]);
  }

  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = SPN_ERR_TARGET_ABI,
    .completion = {
      .target = query.target,
      .host = catalog->host,
      .candidates = candidates,
    },
  });
}

static spn_err_t select_auto(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection) {
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    spn_toolchain_target_t target = sp_zero;
    if (satisfies(entry, catalog, query, &target)) {
      *selection = (spn_toolchain_selection_t) { .toolchain = entry, .target = target };
      return SPN_OK;
    }
  }

  return emit(SPN_ERR_TOOLCHAIN_NONE, catalog, query, SP_NULLPTR, SP_NULLPTR);
}

static spn_err_t select_named(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection) {
  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.toolchain.name);
  if (!toolchain) {
    return emit(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query, satisfying(catalog, query), SP_NULLPTR);
  }

  reach_t reached = reach_first(toolchain, catalog, query.target, query.abis);
  if (reached.err) {
    return emit(reached.err, catalog, query, satisfying(catalog, query), triples(catalog->mem, toolchain->targets));
  }
  if (!usable(toolchain)) {
    return emit(SPN_ERR_TOOLCHAIN_HOST, catalog, query, satisfying(catalog, query), SP_NULLPTR);
  }

  *selection = (spn_toolchain_selection_t) { .toolchain = toolchain, .target = reached.target };
  return SPN_OK;
}

static spn_err_t incomplete_auto(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_da(sp_str_t) names = listing(catalog, query.target);
  if (sp_da_empty(names)) {
    return emit(SPN_ERR_TOOLCHAIN_NONE, catalog, query, names, SP_NULLPTR);
  }
  return emit_abi(catalog, query);
}

static spn_err_t incomplete_named(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.toolchain.name);
  if (!toolchain) {
    return emit(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query, listing(catalog, query.target), SP_NULLPTR);
  }

  reach_t reached = reach_first(toolchain, catalog, query.target, os_abis(query.target.os));
  if (reached.err) {
    return emit(reached.err, catalog, query, listing(catalog, query.target), triples(catalog->mem, toolchain->targets));
  }
  return emit_abi(catalog, query);
}

spn_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection) {
  sp_assert(query.abis.count);
  *selection = sp_zero_s(spn_toolchain_selection_t);
  switch (query.toolchain.kind) {
    case SPN_TOOLCHAIN_REF_AUTO: {
      return select_auto(catalog, query, selection);
    }
    case SPN_TOOLCHAIN_REF_NAMED: {
      return select_named(catalog, query, selection);
    }
    case SPN_TOOLCHAIN_REF_NONE: {
      sp_unreachable_case();
    }
  }

  sp_unreachable_return(SPN_ERROR);
}

spn_err_t spn_toolchain_incomplete(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_assert(!query.abis.count);
  switch (query.toolchain.kind) {
    case SPN_TOOLCHAIN_REF_AUTO: {
      return incomplete_auto(catalog, query);
    }
    case SPN_TOOLCHAIN_REF_NAMED: {
      return incomplete_named(catalog, query);
    }
    case SPN_TOOLCHAIN_REF_NONE: {
      sp_unreachable_case();
    }
  }

  sp_unreachable_return(SPN_ERROR);
}
