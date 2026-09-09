#include "toolchain/catalog.h"

#include "paths/paths.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

static bool has_row(sp_da(spn_toolchain_row_t) rows, spn_triple_t triple) {
  sp_da_for(rows, it) {
    if (spn_triple_equal(rows[it].triple, triple)) {
      return true;
    }
  }
  return false;
}

static void push_row(sp_da(spn_toolchain_row_t)* rows, spn_toolchain_row_t row) {
  if (!has_row(*rows, row.triple)) {
    sp_da_push(*rows, row);
  }
}

// A relative sdk lives inside the artifact; an absolute one is a host path.
static spn_path_t sdk_root(spn_toolchain_catalog_t* catalog, spn_toolchain_support_t support, spn_path_t sdk) {
  switch (support.kind) {
    case SPN_TOOLCHAIN_SUPPORT_ARTIFACT: return sp_fs_is_absolute(sdk.sub) ? sdk : spn_path_join(catalog->mem, spn_toolchain_artifact_root(support.artifact), sdk.sub);
    case SPN_TOOLCHAIN_SUPPORT_LOCAL:
    case SPN_TOOLCHAIN_SUPPORT_NONE: return sdk;
  }
  sp_unreachable_return(sdk);
}

static bool bind_row(spn_toolchain_catalog_t* catalog, spn_toolchain_support_t support, spn_toolchain_target_t target, spn_toolchain_row_t* row) {
  *row = (spn_toolchain_row_t) { .triple = target.triple, .sanitizers = target.sanitizers };
  if (!spn_path_empty(target.sdk)) {
    row->sdk = spn_sdk_at(catalog->mem, target.triple, sdk_root(catalog, support, target.sdk));
    return true;
  }
  return spn_sdk_default(&catalog->sdks, target.triple, &row->sdk);
}

static spn_toolchain_target_t stock(spn_cc_driver_t driver, spn_triple_t triple) {
  return (spn_toolchain_target_t) {
    .triple = triple,
    .sanitizers = spn_toolchain_stock_sanitizers(driver, triple),
  };
}

static void push_stock(sp_da(spn_toolchain_row_t)* rows, spn_toolchain_catalog_t* catalog, const spn_toolchain_decl_t* decl) {
  spn_triple_t host = catalog->host;
  host.abi = host.abi ? host.abi : spn_default_abi(decl->driver, host.os);
  if (!spn_toolchain_driver_composes(decl->driver, spn_ld_dialect(host))) {
    return;
  }

  spn_toolchain_row_t row = sp_zero;
  if (bind_row(catalog, sp_zero_struct(spn_toolchain_support_t), stock(decl->driver, host), &row)) {
    sp_da_push(*rows, row);
  }
  if (host.os != SPN_OS_MACOS || !spn_toolchain_driver_retargets(decl->driver)) {
    return;
  }
  const spn_arch_t* arches = SP_NULLPTR;
  u32 count = spn_os_archs(host.os, &arches);
  sp_for(it, count) {
    if (bind_row(catalog, sp_zero_struct(spn_toolchain_support_t), stock(decl->driver, (spn_triple_t) { arches[it], host.os, host.abi }), &row)) {
      push_row(rows, row);
    }
  }
}

static void push_hosted(sp_da(spn_toolchain_row_t)* rows, spn_toolchain_catalog_t* catalog) {
  static const spn_os_t hosted [] = { SPN_OS_LINUX, SPN_OS_MACOS, SPN_OS_WINDOWS };
  sp_carr_for(hosted, os) {
    const spn_arch_t* arches = SP_NULLPTR;
    const spn_abi_t* abis = SP_NULLPTR;
    u32 num_arches = spn_os_archs(hosted[os], &arches);
    u32 num_abis = spn_os_completions(hosted[os], &abis);
    sp_for(arch, num_arches) {
      sp_for(abi, num_abis) {
        spn_toolchain_row_t row = { .triple = { arches[arch], hosted[os], abis[abi] } };
        if (spn_sdk_served(&catalog->sdks, catalog->host, row.triple, &row.sdk)) {
          push_row(rows, row);
        }
      }
    }
  }
}

static void push_bare(sp_da(spn_toolchain_row_t)* rows, spn_cc_driver_t driver, spn_triple_t host) {
  if (!(spn_toolchain_driver_caps(driver) & SPN_CC_CAP_BARE) || spn_os_format(host.os) != SPN_FORMAT_ELF) {
    return;
  }
  sp_da_push(*rows, ((spn_toolchain_row_t) { .triple = { host.arch, SPN_OS_FREESTANDING, SPN_ABI_BARE } }));
  sp_da_push(*rows, ((spn_toolchain_row_t) { .triple = { host.arch, SPN_OS_LINUX, SPN_ABI_BARE } }));
}

static sp_da(spn_toolchain_row_t) bind_rows(spn_toolchain_catalog_t* catalog, const spn_toolchain_decl_t* decl, spn_toolchain_support_t support) {
  sp_da(spn_toolchain_row_t) rows = sp_da_new(catalog->mem, spn_toolchain_row_t);
  if (!sp_da_empty(decl->targets)) {
    sp_da_for(decl->targets, it) {
      spn_toolchain_row_t row = sp_zero;
      if (bind_row(catalog, support, decl->targets[it], &row)) {
        sp_da_push(rows, row);
      }
    }
    return rows;
  }

  push_stock(&rows, catalog, decl);
  push_bare(&rows, decl->driver, catalog->host);
  if (spn_toolchain_driver_retargets(decl->driver)) {
    push_hosted(&rows, catalog);
  }
  return rows;
}

static bool has_host(sp_da(spn_toolchain_host_t) hosts, spn_triple_t host) {
  sp_da_for(hosts, it) {
    if (spn_triple_match(hosts[it].triple, host)) {
      return true;
    }
  }
  return false;
}

static spn_toolchain_support_t bind_support(spn_toolchain_catalog_t* catalog, const spn_toolchain_decl_t* decl) {
  sp_da(spn_toolchain_host_t) hosts = decl->hosts;
  switch (decl->source) {
    case SPN_TOOLCHAIN_SOURCE_LOCAL: {
      bool supported = sp_da_empty(hosts) || has_host(hosts, catalog->host);
      return (spn_toolchain_support_t) { .kind = supported ? SPN_TOOLCHAIN_SUPPORT_LOCAL : SPN_TOOLCHAIN_SUPPORT_NONE };
    }
    case SPN_TOOLCHAIN_SOURCE_DISTRIBUTION: {
      sp_da_for(hosts, it) {
        if (spn_triple_match(hosts[it].triple, catalog->host)) {
          return (spn_toolchain_support_t) { .kind = SPN_TOOLCHAIN_SUPPORT_ARTIFACT, .artifact = hosts[it].artifact };
        }
      }
      return (spn_toolchain_support_t) { .kind = SPN_TOOLCHAIN_SUPPORT_NONE };
    }
    case SPN_TOOLCHAIN_SOURCE_MIXED: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(sp_zero_struct(spn_toolchain_support_t));
}

static spn_toolchain_info_t bind_toolchain(spn_toolchain_catalog_t* catalog, const spn_toolchain_decl_t* decl) {
  spn_toolchain_support_t support = bind_support(catalog, decl);
  return (spn_toolchain_info_t) {
    .name = decl->name,
    .version = decl->version,
    .driver = decl->driver,
    .compiler = decl->compiler,
    .cxx = decl->cxx,
    .archiver = decl->archiver,
    .lld = decl->lld,
    .link_args = decl->link_args,
    .rows = bind_rows(catalog, decl, support),
    .support = support,
  };
}

void spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, spn_triple_t host, spn_sdk_host_t sdks, sp_mem_t mem) {
  catalog->mem = mem;
  catalog->host = host;
  catalog->sdks = sdks;
  sp_str_om_init(catalog->entries);
}

void spn_toolchain_catalog_add(spn_toolchain_catalog_t* catalog, spn_toolchain_decl_t decl) {
  spn_toolchain_info_t toolchain = bind_toolchain(catalog, &decl);
  spn_toolchain_info_t* existing = spn_toolchain_catalog_get(catalog, toolchain.name);
  if (existing) {
    *existing = toolchain;
    return;
  }
  sp_str_om_insert(catalog->entries, toolchain.name, toolchain);
}

spn_toolchain_info_t* spn_toolchain_catalog_get(spn_toolchain_catalog_t* catalog, sp_str_t name) {
  spn_toolchain_info_t** entry = sp_str_om_getp(catalog->entries, name);
  return entry ? *entry : SP_NULLPTR;
}
