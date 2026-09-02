#include "toolchain/select.h"

#include "ctx/types.h"
#include "enum/enum.h"
#include "error/error.h"
#include "toolchain/catalog.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

SP_PRIVATE spn_triple_t pin_target(spn_toolchain_query_t query) {
  return (spn_triple_t) {
    query.target.arch ? query.target.arch : query.host.arch,
    query.target.os ? query.target.os : query.host.os,
    query.target.abi,
  };
}

SP_PRIVATE bool is_cross(spn_triple_t pinned, spn_triple_t host) {
  return pinned.arch != host.arch || pinned.os != host.os;
}

SP_PRIVATE bool needs_abi(spn_toolchain_query_t query) {
  spn_triple_t pinned = pin_target(query);
  if (pinned.abi) {
    return false;
  }
  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_abis(pinned.os, &abis);
  return count > 1 && is_cross(pinned, query.host);
}

SP_PRIVATE bool has_target(sp_da(spn_triple_t) targets, spn_triple_t target) {
  sp_da_for(targets, it) {
    spn_triple_t entry = targets[it];
    if (entry.arch == target.arch && entry.os == target.os && entry.abi == target.abi) {
      return true;
    }
  }
  return false;
}

SP_PRIVATE bool has_match(sp_da(spn_triple_t) targets, spn_triple_t pattern) {
  sp_da_for(targets, it) {
    if (spn_triple_match(pattern, targets[it])) {
      return true;
    }
  }
  return false;
}

SP_PRIVATE spn_err_t complete(sp_da(spn_triple_t) targets, spn_toolchain_query_t query, spn_triple_t* completed) {
  *completed = sp_zero_s(spn_triple_t);
  spn_triple_t pinned = pin_target(query);

  if (pinned.abi) {
    if (!has_target(targets, pinned)) {
      return SPN_ERR_TOOLCHAIN_TARGET;
    }
    *completed = pinned;
    return SPN_OK;
  }

  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_abis(pinned.os, &abis);

  spn_abi_t primary = SPN_ABI_NONE;
  if (pinned.os == SPN_OS_LINUX) {
    primary = query.shared ? query.host.abi : SPN_ABI_MUSL;
  }

  spn_abi_t ordered [4] = sp_zero;
  u32 num = 0;
  if (primary) {
    ordered[num++] = primary;
  }
  sp_for(it, count) {
    if (abis[it] != primary) {
      ordered[num++] = abis[it];
    }
  }

  sp_for(it, num) {
    spn_triple_t full = { pinned.arch, pinned.os, ordered[it] };
    if (has_target(targets, full)) {
      *completed = full;
      return SPN_OK;
    }
  }
  return SPN_ERR_TOOLCHAIN_TARGET;
}

SP_PRIVATE sp_da(sp_str_t) render_targets(sp_mem_t mem, sp_da(spn_triple_t) targets) {
  sp_da(sp_str_t) rendered = sp_da_new(mem, sp_str_t);
  sp_da_for(targets, it) {
    sp_da_push(rendered, spn_triple_to_str(mem, targets[it]));
  }
  return rendered;
}

SP_PRIVATE bool host_supported(spn_toolchain_info_t* toolchain, spn_triple_t host) {
  if (sp_da_empty(toolchain->hosts)) {
    return true;
  }
  sp_da_for(toolchain->hosts, it) {
    if (spn_triple_match(toolchain->hosts[it].triple, host)) {
      return true;
    }
  }
  return false;
}

spn_opt_artifact_t get_artifact(sp_da(spn_toolchain_host_t) hosts, spn_triple_t host) {
  spn_opt_artifact_t result = sp_zero;

  sp_da_for(hosts, it) {
    if (spn_triple_match(hosts[it].triple, host) && !sp_str_empty(hosts[it].artifact.url)) {
      sp_opt_set(result, hosts[it].artifact);
      return result;
    }
  }

  return result;
}

SP_PRIVATE spn_err_t try_toolchain(spn_toolchain_info_t* toolchain, sp_da(spn_triple_t) targets, spn_toolchain_query_t query, spn_toolchain_resolution_t* resolution) {
  spn_triple_t completed = sp_zero;
  spn_err_t err = complete(targets, query, &completed);
  if (err) {
    switch (query.role) {
      case SPN_TOOLCHAIN_ROLE_BUILD:  return err;
      case SPN_TOOLCHAIN_ROLE_SCRIPT: return SPN_ERR_TOOLCHAIN_SCRIPT_TARGET;
    }
  }

  spn_opt_artifact_t artifact = sp_zero;
  switch (toolchain->source) {
    case SPN_TOOLCHAIN_SOURCE_DISTRIBUTION: {
      artifact = get_artifact(toolchain->hosts, query.host);
      if (sp_opt_is_null(artifact)) {
        return SPN_ERR_TOOLCHAIN_HOST;
      }
      break;
    }
    case SPN_TOOLCHAIN_SOURCE_LOCAL: {
      if (!host_supported(toolchain, query.host)) {
        return SPN_ERR_TOOLCHAIN_HOST;
      }
      break;
    }
  }

  resolution->info = toolchain;
  resolution->artifact = artifact;
  resolution->triple = completed;
  return SPN_OK;
}

SP_PRIVATE spn_err_t make_error(spn_err_t kind, spn_toolchain_catalog_t* catalog, sp_da(spn_triple_t) targets, spn_toolchain_query_t query, sp_mem_t mem) {
  sp_da(sp_str_t) candidates = sp_da_new(mem, sp_str_t);
  sp_om_for(catalog->entries, it) {
    spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
    spn_toolchain_resolution_t resolution = sp_zero;
    if (!try_toolchain(entry, spn_toolchain_targets(mem, entry, query.host), query, &resolution)) {
      sp_da_push(candidates, entry->name);
    }
  }

  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = kind,
    .toolchain = {
      .name = query.name,
      .target = pin_target(query),
      .host = query.host,
      .candidates = candidates,
      .targets = render_targets(mem, targets),
    },
  });
}

SP_PRIVATE spn_err_t make_abi_error(spn_toolchain_query_t query, sp_mem_t mem) {
  spn_triple_t pinned = pin_target(query);
  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_abis(pinned.os, &abis);

  sp_da(sp_str_t) candidates = sp_da_new(mem, sp_str_t);
  sp_for(it, count) {
    sp_da_push(candidates, spn_abi_to_str(abis[it]));
  }

  return spn_err_emit(&spn, (spn_err_union_t) {
    .kind = SPN_ERR_TARGET_ABI,
    .completion = {
      .target = pinned,
      .host = query.host,
      .candidates = candidates,
    },
  });
}

spn_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_mem_t mem, spn_toolchain_resolution_t* resolution) {
  *resolution = (spn_toolchain_resolution_t)sp_zero;

  if (sp_str_equal_cstr(query.name, "auto")) {
    if (needs_abi(query)) {
      return make_abi_error(query, mem);
    }
    sp_om_for(catalog->entries, it) {
      spn_toolchain_info_t* entry = sp_om_at(catalog->entries, it);
      if (!try_toolchain(entry, spn_toolchain_targets(mem, entry, query.host), query, resolution)) {
        return SPN_OK;
      }
    }
    return make_error(SPN_ERR_TOOLCHAIN_NONE, catalog, SP_NULLPTR, query, mem);
  }

  spn_toolchain_info_t* toolchain = spn_toolchain_catalog_get(catalog, query.name);
  if (!toolchain) {
    return make_error(SPN_ERR_TOOLCHAIN_UNKNOWN, catalog, SP_NULLPTR, query, mem);
  }

  sp_da(spn_triple_t) targets = spn_toolchain_targets(mem, toolchain, query.host);
  if (needs_abi(query) && has_match(targets, pin_target(query))) {
    return make_abi_error(query, mem);
  }

  spn_err_t err = try_toolchain(toolchain, targets, query, resolution);
  if (err) {
    return make_error(err, catalog, targets, query, mem);
  }
  return SPN_OK;
}
