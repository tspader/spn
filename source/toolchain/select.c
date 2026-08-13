#include "toolchain/select.h"

#include "ctx/types.h"
#include "error/error.h"
#include "toolchain/catalog.h"
#include "triple/triple.h"

bool is_supported(spn_toolchain_info_t* toolchain, spn_triple_t target, spn_triple_t host) {
  if (sp_da_empty(toolchain->targets)) {
    return spn_triple_match(target, host) && spn_triple_match(host, target);
  }

  sp_da_for(toolchain->targets, it) {
    if (spn_triple_match(toolchain->targets[it], target)) {
      return true;
    }
  }

  return false;
}

spn_opt_artifact_t get_artifact(sp_da(spn_toolchain_host_t) hosts, spn_triple_t host) {
  spn_opt_artifact_t result = sp_zero;

  sp_da_for(hosts, it) {
    if (spn_triple_match(hosts[it].triple, host)) {
      sp_opt_set(result, hosts[it].artifact);
      return result;
    }
  }

  return result;
}

SP_PRIVATE spn_err_t try_toolchain(spn_toolchain_info_t* toolchain, spn_toolchain_query_t query, spn_toolchain_resolution_t* resolution) {
  if (!is_supported(toolchain, query.target, query.host)) {
    switch (query.role) {
      case SPN_TOOLCHAIN_ROLE_BUILD:  return SPN_ERR_TOOLCHAIN_TARGET;
      case SPN_TOOLCHAIN_ROLE_SCRIPT: return SPN_ERR_TOOLCHAIN_SCRIPT_TARGET;
    }
  }

  spn_opt_artifact_t artifact = sp_zero;
  if (toolchain->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION) {
    artifact = get_artifact(toolchain->hosts, query.host);
    if (sp_opt_is_null(artifact)) {
      return SPN_ERR_TOOLCHAIN_HOST;
    }
  }

  resolution->info = toolchain;
  resolution->artifact = artifact;
  return SPN_OK;
}

SP_PRIVATE spn_err_t make_error(spn_err_t kind, spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_mem_t mem) {
  sp_da(sp_str_t) candidates = sp_da_new(mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* toolchain = sp_om_at(catalog->entries, it);
    if (is_supported(toolchain, query.target, query.host)) {
      sp_da_push(candidates, toolchain->name);
    }
  }

  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = kind,
    .toolchain = {
      .name = query.name,
      .target = query.target,
      .host = query.host,
      .candidates = candidates,
    },
  });
}

spn_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_mem_t mem, spn_toolchain_resolution_t* resolution) {
  *resolution = (spn_toolchain_resolution_t)sp_zero;

  if (sp_str_equal_cstr(query.name, "auto")) {
    sp_om_for(catalog->entries, it) {
      if (!try_toolchain(sp_om_at(catalog->entries, it), query, resolution)) {
        return SPN_OK;
      }
    }
    return make_error(SPN_ERR_TOOLCHAIN_NONE, catalog, query, mem);
  }

  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.name);
  if (!toolchain) {
    return make_error(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query, mem);
  }

  spn_err_t err = try_toolchain(toolchain, query, resolution);
  if (err) {
    return make_error(err, catalog, query, mem);
  }
  return SPN_OK;
}
