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

spn_err_union_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_resolution_t* resolution) {
  *resolution = (spn_toolchain_resolution_t)sp_zero;
  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.name);
  if (!toolchain) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_TOOLCHAIN_UNKNOWN,
      .toolchain = {
        .role = query.role,
        .name = query.name,
        .host = query.host,
        .catalog = catalog,
      },
    };
  }

  if (!spn_toolchain_supports(toolchain, query.target, query.host)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_TOOLCHAIN_TARGET,
      .toolchain = {
        .role = query.role,
        .name = query.name,
        .target = query.target,
        .host = query.host,
        .catalog = catalog,
      },
    };
  }

  resolution->info = toolchain;
  if (toolchain->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION) {
    resolution->artifact = spn_toolchain_select_artifact(toolchain->hosts, query.host);
    if (sp_opt_is_null(resolution->artifact)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_TOOLCHAIN_HOST,
        .toolchain = {
          .role = query.role,
          .name = query.name,
          .target = query.target,
          .host = query.host,
          .catalog = catalog,
        },
      };
    }
  }
  return spn_result(SPN_OK);
}
