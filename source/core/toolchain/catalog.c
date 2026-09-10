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

static bool has_triple(sp_da(spn_triple_t) triples, spn_triple_t triple) {
  sp_da_for(triples, it) {
    if (spn_triple_equal(triples[it], triple)) {
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

static spn_path_t sdk_root(spn_toolchain_catalog_t* catalog, spn_toolchain_support_t support, spn_path_t sdk) {
  switch (support.kind) {
    case SPN_TOOLCHAIN_SUPPORT_ARTIFACT: return sp_fs_is_absolute(sdk.sub) ? sdk : spn_path_join(catalog->mem, spn_toolchain_artifact_root(support.artifact), sdk.sub);
    case SPN_TOOLCHAIN_SUPPORT_LOCAL:
    case SPN_TOOLCHAIN_SUPPORT_NONE: return sdk;
  }
  sp_unreachable_return(sdk);
}

static void bind(spn_toolchain_catalog_t* catalog, spn_toolchain_info_t* info, spn_toolchain_support_t support, spn_toolchain_target_t target) {
  if (has_row(info->rows, target.triple) || has_triple(info->unserved, target.triple)) {
    return;
  }
  spn_toolchain_row_t row = { .triple = target.triple, .sanitizers = target.sanitizers };
  if (!spn_path_empty(target.sdk)) {
    row.sdk = spn_sdk_at(catalog->mem, target.triple, sdk_root(catalog, support, target.sdk));
    sp_da_push(info->rows, row);
    return;
  }
  if (spn_sdk_default(&catalog->sdks, target.triple, &row.sdk)) {
    sp_da_push(info->rows, row);
    return;
  }
  sp_da_push(info->unserved, target.triple);
}

static spn_toolchain_target_t stock(spn_cc_driver_t driver, spn_triple_t triple) {
  return (spn_toolchain_target_t) {
    .triple = triple,
    .sanitizers = spn_toolchain_stock_sanitizers(driver, triple),
  };
}

static void push_stock(spn_toolchain_catalog_t* catalog, spn_toolchain_info_t* info, const spn_toolchain_decl_t* decl) {
  spn_triple_t host = catalog->host;
  host.abi = host.abi ? host.abi : spn_default_abi(decl->driver, host.os);
  if (!host.abi || !spn_toolchain_driver_composes(decl->driver, spn_ld_dialect(host))) {
    return;
  }
  bind(catalog, info, sp_zero_struct(spn_toolchain_support_t), stock(decl->driver, host));
}

static void push_hosted(sp_da(spn_toolchain_row_t)* rows, spn_toolchain_catalog_t* catalog, spn_cc_driver_t driver) {
  static const spn_os_t hosted [] = { SPN_OS_LINUX, SPN_OS_MACOS, SPN_OS_WINDOWS };
  sp_carr_for(hosted, os) {
    const spn_arch_t* arches = SP_NULLPTR;
    const spn_abi_t* abis = SP_NULLPTR;
    u32 num_arches = spn_os_archs(hosted[os], &arches);
    u32 num_abis = spn_os_completions(hosted[os], &abis);
    bool own = hosted[os] == catalog->host.os;
    sp_for(arch, num_arches) {
      sp_for(abi, num_abis) {
        spn_toolchain_row_t row = { .triple = { arches[arch], hosted[os], abis[abi] } };
        if (own) {
          row.sanitizers = spn_toolchain_stock_sanitizers(driver, row.triple);
        }
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
  push_row(rows, (spn_toolchain_row_t) { .triple = { host.arch, SPN_OS_FREESTANDING, SPN_ABI_BARE } });
  push_row(rows, (spn_toolchain_row_t) { .triple = { host.arch, SPN_OS_LINUX, SPN_ABI_BARE } });
}

static void bind_rows(spn_toolchain_catalog_t* catalog, const spn_toolchain_decl_t* decl, spn_toolchain_support_t support, spn_toolchain_info_t* info) {
  info->rows = sp_da_new(catalog->mem, spn_toolchain_row_t);
  info->unserved = sp_da_new(catalog->mem, spn_triple_t);
  sp_da_for(decl->targets, it) {
    bind(catalog, info, support, decl->targets[it]);
  }
  if (!decl->host_row) {
    return;
  }

  push_stock(catalog, info, decl);
  push_bare(&info->rows, decl->driver, catalog->host);
  if (spn_toolchain_driver_retargets(decl->driver)) {
    push_hosted(&info->rows, catalog, decl->driver);
  }
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
  spn_toolchain_info_t info = {
    .name = decl->name,
    .version = decl->version,
    .driver = decl->driver,
    .compiler = decl->compiler,
    .cxx = decl->cxx,
    .archiver = decl->archiver,
    .lld = decl->lld,
    .link_args = decl->link_args,
    .support = bind_support(catalog, decl),
  };
  bind_rows(catalog, decl, info.support, &info);
  return info;
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
