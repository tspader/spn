#include "toolchain/select.h"

#include "ctx/types.h"
#include "error/error.h"
#include "toolchain/catalog.h"
#include "triple/triple.h"

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
  return toolchain->support.kind != SPN_TOOLCHAIN_SUPPORT_NONE && complete(toolchain, query, triple);
}

static bool could_satisfy(const spn_toolchain_info_t* toolchain, spn_toolchain_query_t query) {
  spn_triple_t triple = sp_zero;
  if (query.abis.count) {
    return satisfies(toolchain, query, &triple);
  }
  return toolchain->support.kind != SPN_TOOLCHAIN_SUPPORT_NONE && reaches(toolchain, query.target);
}

static sp_da(sp_str_t) candidates(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  sp_da(sp_str_t) names = sp_da_new(catalog->mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    if (could_satisfy(entry, query)) {
      sp_da_push(names, entry->name);
    }
  }
  return names;
}

static spn_err_t emit(spn_err_t kind, spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_da(sp_str_t) candidates, sp_da(spn_triple_t) targets) {
  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = kind,
    .toolchain = {
      .name = query.name,
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

static bool reachable(spn_toolchain_catalog_t* catalog, spn_triple_t target) {
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    if (entry->support.kind != SPN_TOOLCHAIN_SUPPORT_NONE && reaches(entry, target)) {
      return true;
    }
  }
  return false;
}

static spn_err_t select_auto(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection) {
  if (!query.abis.count) {
    if (reachable(catalog, query.target)) {
      return emit_abi(catalog, query);
    }
    return emit(SPN_ERR_TOOLCHAIN_NONE, catalog, query, SP_NULLPTR, SP_NULLPTR);
  }

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
  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.name);
  if (!toolchain) {
    return emit(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query, candidates(catalog, query), SP_NULLPTR);
  }
  if (!query.abis.count) {
    if (reaches(toolchain, query.target)) {
      return emit_abi(catalog, query);
    }
    return emit(SPN_ERR_TOOLCHAIN_TARGET, catalog, query, candidates(catalog, query), toolchain->targets);
  }

  spn_triple_t triple = sp_zero;
  if (!complete(toolchain, query, &triple)) {
    return emit(SPN_ERR_TOOLCHAIN_TARGET, catalog, query, candidates(catalog, query), toolchain->targets);
  }
  if (toolchain->support.kind == SPN_TOOLCHAIN_SUPPORT_NONE) {
    return emit(SPN_ERR_TOOLCHAIN_HOST, catalog, query, candidates(catalog, query), SP_NULLPTR);
  }

  *selection = (spn_toolchain_selection_t) { .toolchain = toolchain, .triple = triple };
  return SPN_OK;
}

spn_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection) {
  *selection = sp_zero_s(spn_toolchain_selection_t);
  switch (query.kind) {
    case SPN_TOOLCHAIN_QUERY_AUTO: {
      return select_auto(catalog, query, selection);
    }
    case SPN_TOOLCHAIN_QUERY_NAMED: {
      return select_named(catalog, query, selection);
    }
  }

  sp_unreachable_return(SPN_ERROR);
}
