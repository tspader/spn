#include "toolchain/select.h"

#include "ctx/types.h"
#include "error/error.h"
#include "toolchain/catalog.h"
#include "triple/triple.h"

static bool usable(const spn_toolchain_info_t* toolchain) {
  return toolchain->support.kind != SPN_TOOLCHAIN_SUPPORT_NONE;
}

static bool supports(const spn_toolchain_info_t* toolchain, spn_triple_t triple) {
  sp_da_for(toolchain->targets, it) {
    if (spn_triple_equal(toolchain->targets[it], triple)) {
      return true;
    }
  }
  return false;
}

static bool reaches(const spn_toolchain_info_t* toolchain, spn_triple_t target) {
  spn_triple_t pattern = { target.arch, target.os, SPN_ABI_NONE };
  sp_da_for(toolchain->targets, it) {
    if (spn_triple_match(pattern, toolchain->targets[it])) {
      return true;
    }
  }
  return false;
}

static bool complete(const spn_toolchain_info_t* toolchain, spn_toolchain_query_t query, spn_triple_t* triple) {
  sp_for(it, query.abis.count) {
    spn_triple_t candidate = { query.target.arch, query.target.os, query.abis.items[it] };
    if (supports(toolchain, candidate)) {
      *triple = candidate;
      return true;
    }
  }
  return false;
}

static bool satisfies(const spn_toolchain_info_t* toolchain, spn_toolchain_query_t query, spn_triple_t* triple) {
  return usable(toolchain) && complete(toolchain, query, triple);
}

static sp_da(sp_str_t) satisfying(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_da(sp_str_t) names = sp_da_new(catalog->mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    spn_triple_t triple = sp_zero;
    if (satisfies(entry, query, &triple)) {
      sp_da_push(names, entry->name);
    }
  }
  return names;
}

static sp_da(sp_str_t) reaching(spn_toolchain_catalog_t* catalog, spn_triple_t target) {
  sp_da(sp_str_t) names = sp_da_new(catalog->mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    if (usable(entry) && reaches(entry, target)) {
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
  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_abis(query.target.os, &abis);
  sp_da(spn_abi_t) candidates = sp_da_new(catalog->mem, spn_abi_t);
  sp_for(it, count) {
    sp_da_push(candidates, abis[it]);
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
    spn_triple_t triple = sp_zero;
    if (satisfies(entry, query, &triple)) {
      *selection = (spn_toolchain_selection_t) { .toolchain = entry, .triple = triple };
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

  spn_triple_t triple = sp_zero;
  if (!complete(toolchain, query, &triple)) {
    return emit(SPN_ERR_TOOLCHAIN_TARGET, catalog, query, satisfying(catalog, query), toolchain->targets);
  }
  if (!usable(toolchain)) {
    return emit(SPN_ERR_TOOLCHAIN_HOST, catalog, query, satisfying(catalog, query), SP_NULLPTR);
  }

  *selection = (spn_toolchain_selection_t) { .toolchain = toolchain, .triple = triple };
  return SPN_OK;
}

static spn_err_t incomplete_auto(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_da(sp_str_t) names = reaching(catalog, query.target);
  if (sp_da_empty(names)) {
    return emit(SPN_ERR_TOOLCHAIN_NONE, catalog, query, names, SP_NULLPTR);
  }
  return emit_abi(catalog, query);
}

static spn_err_t incomplete_named(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.toolchain.name);
  if (!toolchain) {
    return emit(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query, reaching(catalog, query.target), SP_NULLPTR);
  }
  if (!reaches(toolchain, query.target)) {
    return emit(SPN_ERR_TOOLCHAIN_TARGET, catalog, query, reaching(catalog, query.target), toolchain->targets);
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
