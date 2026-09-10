#include "toolchain/select.h"

#include "ctx/types.h"
#include "error/error.h"
#include "toolchain/catalog.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

typedef enum {
  REACH_UNSERVED,
  REACH_UNLISTED,
  REACH_DROPPED,
  REACH_SANITIZERS,
  REACH_OK,
} reach_kind_t;

typedef struct {
  reach_kind_t kind;
  spn_err_t err;
  spn_toolchain_row_t row;
  spn_sanitizer_set_t unsupported;
} reach_t;

static bool usable(const spn_toolchain_info_t* toolchain) {
  return toolchain->support.kind != SPN_TOOLCHAIN_SUPPORT_NONE;
}

static const spn_toolchain_row_t* listed(const spn_toolchain_info_t* toolchain, spn_triple_t triple) {
  sp_da_for(toolchain->rows, it) {
    if (spn_triple_equal(toolchain->rows[it].triple, triple)) {
      return &toolchain->rows[it];
    }
  }
  return SP_NULLPTR;
}

static sp_da(spn_triple_t) triples(sp_mem_t mem, sp_da(spn_toolchain_row_t) rows) {
  sp_da(spn_triple_t) out = sp_da_new(mem, spn_triple_t);
  sp_da_for(rows, it) {
    sp_da_push(out, rows[it].triple);
  }
  return out;
}

static spn_triple_t with_abi(spn_triple_t target, spn_abi_t abi) {
  return (spn_triple_t) { target.arch, target.os, abi };
}

static spn_abi_list_t completions(spn_os_t os) {
  spn_abi_list_t list = sp_zero;
  const spn_abi_t* abis = SP_NULLPTR;
  list.count = spn_os_completions(os, &abis);
  sp_for(it, list.count) {
    list.items[it] = abis[it];
  }
  return list;
}

static bool lists_completion(const spn_toolchain_info_t* toolchain, spn_triple_t target) {
  spn_abi_list_t abis = completions(target.os);
  sp_for(it, abis.count) {
    if (listed(toolchain, with_abi(target, abis.items[it]))) {
      return true;
    }
  }
  return false;
}

static reach_t supports(spn_toolchain_row_t row, spn_toolchain_query_t query) {
  spn_sanitizer_set_t missing = query.sanitizers & ~row.sanitizers;
  if (missing) {
    return (reach_t) { .kind = REACH_SANITIZERS, .err = SPN_ERR_SANITIZER_UNSUPPORTED, .row = row, .unsupported = missing };
  }
  spn_sanitizer_set_t heavy = query.sanitizers & ~SPN_SANITIZER_UNDEFINED;
  spn_linkage_t linkage = query.linkage ? query.linkage : spn_abi_linkage(row.triple.abi);
  if (heavy && linkage == SPN_LIB_KIND_STATIC && spn_ld_static(spn_ld_dialect(row.triple))) {
    return (reach_t) { .kind = REACH_SANITIZERS, .err = SPN_ERR_SANITIZER_STATIC, .row = row, .unsupported = heavy };
  }
  return (reach_t) { .kind = REACH_OK, .row = row };
}

static reach_kind_t absence(const spn_toolchain_catalog_t* catalog, const spn_toolchain_info_t* toolchain, spn_triple_t triple) {
  if (spn_triple_in(toolchain->unserved, triple)) {
    return REACH_DROPPED;
  }
  spn_sdk_t sdk = sp_zero;
  if (spn_toolchain_driver_retargets(toolchain->driver) && !spn_sdk_served(&catalog->sdks, catalog->host, triple, &sdk)) {
    return REACH_UNSERVED;
  }
  return REACH_UNLISTED;
}

static reach_t attempt(const spn_toolchain_catalog_t* catalog, const spn_toolchain_info_t* toolchain, spn_toolchain_query_t query, spn_triple_t candidate) {
  const spn_toolchain_row_t* row = listed(toolchain, candidate);
  if (row) {
    return supports(*row, query);
  }
  return (reach_t) { .kind = absence(catalog, toolchain, candidate), .row = { .triple = candidate } };
}

static bool reached(reach_t reach) {
  return reach.kind == REACH_OK;
}

static bool closer(reach_t a, reach_t b) {
  return a.kind > b.kind;
}

static reach_t reach_best(const spn_toolchain_catalog_t* catalog, const spn_toolchain_info_t* toolchain, spn_toolchain_query_t query, spn_abi_list_t abis) {
  reach_t best = attempt(catalog, toolchain, query, with_abi(query.target, abis.items[0]));
  sp_for_range(it, 1, abis.count) {
    if (reached(best)) {
      break;
    }
    reach_t reach = attempt(catalog, toolchain, query, with_abi(query.target, abis.items[it]));
    if (closer(reach, best)) {
      best = reach;
    }
  }
  return best;
}

static spn_err_t sdk_refusal(spn_triple_t target) {
  switch (spn_sdk_kind(target)) {
    case SPN_SDK_SYSROOT: return SPN_ERR_TOOLCHAIN_SYSROOT;
    case SPN_SDK_MACOS: return SPN_ERR_TOOLCHAIN_SDK_MACOS;
    case SPN_SDK_MSVC: return SPN_ERR_TOOLCHAIN_SDK_MSVC;
    case SPN_SDK_NONE: sp_unreachable_case();
  }
  sp_unreachable_return(SPN_ERR_TOOLCHAIN_TARGET);
}

static bool satisfies(const spn_toolchain_catalog_t* catalog, const spn_toolchain_info_t* toolchain, spn_toolchain_query_t query, spn_toolchain_row_t* row) {
  if (!usable(toolchain)) {
    return false;
  }
  reach_t reach = reach_best(catalog, toolchain, query, query.abis);
  *row = reach.row;
  return reached(reach);
}

static sp_da(sp_str_t) satisfying(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_da(sp_str_t) names = sp_da_new(catalog->mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    spn_toolchain_row_t row = sp_zero;
    if (satisfies(catalog, entry, query, &row)) {
      sp_da_push(names, entry->name);
    }
  }
  return names;
}

static sp_da(sp_str_t) listing(spn_toolchain_catalog_t* catalog, spn_triple_t target) {
  sp_da(sp_str_t) names = sp_da_new(catalog->mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    if (usable(entry) && lists_completion(entry, target)) {
      sp_da_push(names, entry->name);
    }
  }
  return names;
}

static spn_err_t emit(spn_err_t kind, spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_triple_t target, sp_da(sp_str_t) candidates, sp_da(spn_triple_t) targets) {
  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = kind,
    .toolchain = {
      .name = query.toolchain.name,
      .target = target,
      .host = catalog->host,
      .sanitizers = query.sanitizers,
      .candidates = candidates,
      .targets = targets,
    },
  });
}

static spn_err_t emit_reach(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, const spn_toolchain_info_t* toolchain, reach_t reach, sp_da(sp_str_t) candidates) {
  switch (reach.kind) {
    case REACH_SANITIZERS: {
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = reach.err,
        .sanitizer = {
          .toolchain = toolchain->name,
          .target = reach.row.triple,
          .unsupported = reach.unsupported,
          .supported = reach.row.sanitizers,
        },
      });
    }
    case REACH_DROPPED:
    case REACH_UNSERVED: {
      return emit(sdk_refusal(reach.row.triple), catalog, query, reach.row.triple, candidates, triples(catalog->mem, toolchain->rows));
    }
    case REACH_UNLISTED: {
      return emit(SPN_ERR_TOOLCHAIN_TARGET, catalog, query, query.target, candidates, triples(catalog->mem, toolchain->rows));
    }
    case REACH_OK: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t emit_abi(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  spn_abi_list_t abis = completions(query.target.os);
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
    spn_toolchain_row_t row = sp_zero;
    if (satisfies(catalog, entry, query, &row)) {
      *selection = (spn_toolchain_selection_t) { .toolchain = entry, .row = row };
      return SPN_OK;
    }
  }

  return emit(SPN_ERR_TOOLCHAIN_NONE, catalog, query, query.target, SP_NULLPTR, SP_NULLPTR);
}

static spn_err_t select_named(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection) {
  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.toolchain.name);
  if (!toolchain) {
    return emit(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query, query.target, satisfying(catalog, query), SP_NULLPTR);
  }
  if (!usable(toolchain)) {
    return emit(SPN_ERR_TOOLCHAIN_HOST, catalog, query, query.target, satisfying(catalog, query), SP_NULLPTR);
  }

  reach_t reach = reach_best(catalog, toolchain, query, query.abis);
  if (!reached(reach)) {
    return emit_reach(catalog, query, toolchain, reach, satisfying(catalog, query));
  }

  *selection = (spn_toolchain_selection_t) { .toolchain = toolchain, .row = reach.row };
  return SPN_OK;
}

static spn_err_t incomplete_auto(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_da(sp_str_t) names = listing(catalog, query.target);
  if (sp_da_empty(names)) {
    return emit(SPN_ERR_TOOLCHAIN_NONE, catalog, query, query.target, names, SP_NULLPTR);
  }
  return emit_abi(catalog, query);
}

static spn_err_t incomplete_named(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.toolchain.name);
  if (!toolchain) {
    return emit(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query, query.target, listing(catalog, query.target), SP_NULLPTR);
  }
  if (!usable(toolchain)) {
    return emit(SPN_ERR_TOOLCHAIN_HOST, catalog, query, query.target, listing(catalog, query.target), SP_NULLPTR);
  }

  reach_t reach = reach_best(catalog, toolchain, query, completions(query.target.os));
  if (!reached(reach)) {
    return emit_reach(catalog, query, toolchain, reach, listing(catalog, query.target));
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
