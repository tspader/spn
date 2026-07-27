#include "toolchain/select.h"

#include "toolchain/catalog.h"
#include "triple/triple.h"

bool spn_toolchain_supports(spn_toolchain_info_t* toolchain, spn_triple_t target, spn_triple_t host) {
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

SP_PRIVATE spn_err_t spn_toolchain_try(spn_toolchain_info_t* toolchain, spn_toolchain_query_t query, spn_toolchain_resolution_t* resolution) {
  if (!spn_toolchain_supports(toolchain, query.target, query.host)) {
    return SPN_ERR_TOOLCHAIN_TARGET;
  }

  spn_opt_artifact_t artifact = sp_zero;
  if (toolchain->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION) {
    artifact = spn_toolchain_select_artifact(toolchain->hosts, query.host);
    if (sp_opt_is_null(artifact)) {
      return SPN_ERR_TOOLCHAIN_HOST;
    }
  }

  resolution->info = toolchain;
  resolution->artifact = artifact;
  return SPN_OK;
}

SP_PRIVATE spn_err_union_t spn_toolchain_err(spn_err_t kind, spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query) {
  return (spn_err_union_t) {
    .kind = kind,
    .toolchain = {
      .role = query.role,
      .name = query.name,
      .target = query.target,
      .host = query.host,
      .catalog = catalog,
    },
  };
}

spn_err_union_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_resolution_t* resolution) {
  *resolution = (spn_toolchain_resolution_t)sp_zero;

  if (sp_str_equal_cstr(query.name, "auto")) {
    sp_om_for(catalog->entries, it) {
      if (!spn_toolchain_try(sp_om_at(catalog->entries, it), query, resolution)) {
        return spn_result(SPN_OK);
      }
    }
    return spn_toolchain_err(SPN_ERR_TOOLCHAIN_NONE, catalog, query);
  }

  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.name);
  if (!toolchain) {
    return spn_toolchain_err(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, query);
  }

  spn_err_t err = spn_toolchain_try(toolchain, query, resolution);
  if (err) {
    return spn_toolchain_err(err, catalog, query);
  }
  return spn_result(SPN_OK);
}
