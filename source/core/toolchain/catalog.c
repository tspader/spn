#include "toolchain/catalog.h"

#include "paths/paths.h"
#include "toolchain/toolchain.h"
#include "toolchains.gen.h"
#include "triple/triple.h"

static spn_err_t load_launcher(const spn_cg_launcher_t* in, spn_toolchain_source_t source, spn_toolchain_launcher_t* launcher) {
  launcher->args = in->args;
  if (spn_toolchain_program(source, SPN_PATH_ROOT_NONE, in->program, &launcher->program) != SPN_PATH_OK) {
    return SPN_ERROR;
  }
  return SPN_OK;
}

static spn_err_t load_target(const spn_cg_toolchain_target_t* in, spn_cc_driver_t driver, spn_toolchain_source_t source, spn_toolchain_target_t* target) {
  spn_triple_t partial = {
    .arch = sp_opt_is_null(in->arch) ? SPN_ARCH_NONE : sp_opt_get(in->arch),
    .os = sp_opt_is_null(in->os) ? SPN_OS_NONE : sp_opt_get(in->os),
    .abi = sp_opt_is_null(in->abi) ? SPN_ABI_NONE : sp_opt_get(in->abi),
  };
  if (spn_triple_entry(partial, &target->triple) != SPN_TRIPLE_ENTRY_OK) {
    return SPN_ERROR;
  }
  if (!spn_toolchain_driver_composes(driver, spn_ld_dialect(target->triple))) {
    return SPN_ERROR;
  }
  if (sp_str_empty(in->sdk)) {
    return SPN_OK;
  }
  if (!spn_sdk_declarable(spn_sdk_kind(target->triple))) {
    return SPN_ERROR;
  }
  if (spn_toolchain_path(source, SPN_PATH_ROOT_NONE, in->sdk, &target->sdk) != SPN_PATH_OK) {
    return SPN_ERROR;
  }
  return SPN_OK;
}

spn_err_t spn_toolchain_decls_parse(sp_mem_t mem, sp_str_t json, sp_da(spn_toolchain_decl_t)* decls) {
  spn_cg_toolchains_t root = sp_zero;
  if (!spn_toolchains_read(json, &root, mem)) {
    return SPN_ERROR;
  }

  *decls = sp_da_new(mem, spn_toolchain_decl_t);
  sp_om_for(root.toolchain, it) {
    const spn_cg_toolchain_t* t = sp_om_at(root.toolchain, it);

    spn_toolchain_decl_t decl = sp_zero;
    decl.name = t->name;
    decl.version = t->version;
    decl.driver = t->driver;
    decl.linker = sp_opt_is_null(t->linker) ? SPN_LD_FAMILY_NONE : sp_opt_get(t->linker);
    if (!spn_ld_accepts(decl.driver, decl.linker)) {
      return SPN_ERROR;
    }
    decl.link_args = t->link_args;

    decl.hosts = sp_da_new(mem, spn_toolchain_host_t);
    sp_da_for(t->host, it) {
      spn_triple_t host = sp_zero;
      if (spn_triple_parse_host(t->host[it].key, &host)) {
        return SPN_ERROR;
      }
      sp_da_push(decl.hosts, ((spn_toolchain_host_t) {
        .triple = host,
        .artifact = {
          .url = t->host[it].value.url,
          .sha256 = t->host[it].value.sha256,
          .mirror_list = t->mirrors,
        },
      }));
    }
    decl.source = spn_toolchain_source(decl.hosts);
    if (decl.source == SPN_TOOLCHAIN_SOURCE_MIXED) {
      return SPN_ERROR;
    }
    spn_try(load_launcher(&t->compiler, decl.source, &decl.compiler));
    spn_try(load_launcher(&t->archiver, decl.source, &decl.archiver));
    spn_try(load_launcher(&t->cxx, decl.source, &decl.cxx));

    decl.targets = sp_da_new(mem, spn_toolchain_target_t);
    sp_da_for(t->target, it) {
      spn_toolchain_target_t target = sp_zero;
      spn_try(load_target(&t->target[it], decl.driver, decl.source, &target));
      sp_da_push(decl.targets, target);
    }

    sp_da_push(*decls, decl);
  }

  return SPN_OK;
}

static sp_da(spn_toolchain_target_t) default_targets(spn_toolchain_catalog_t* catalog, const spn_toolchain_decl_t* decl) {
  spn_triple_t host = catalog->host;
  host.abi = host.abi ? host.abi : spn_default_abi(decl->driver, host.os);

  sp_da(spn_toolchain_target_t) targets = sp_da_new(catalog->mem, spn_toolchain_target_t);
  if (!spn_toolchain_driver_composes(decl->driver, spn_ld_dialect(host))) {
    return targets;
  }
  sp_da_push(targets, ((spn_toolchain_target_t) { .triple = host }));
  if (spn_os_format(host.os) == SPN_FORMAT_ELF && !spn_toolchain_driver_retargets(decl->driver)) {
    sp_da_push(targets, ((spn_toolchain_target_t) { .triple = { host.arch, SPN_OS_FREESTANDING, SPN_ABI_BARE } }));
  }
  return targets;
}

static sp_da(spn_toolchain_target_t) declared_targets(spn_toolchain_catalog_t* catalog, sp_da(spn_toolchain_target_t) declared, spn_toolchain_support_t support) {
  switch (support.kind) {
    case SPN_TOOLCHAIN_SUPPORT_ARTIFACT: {
      spn_path_t root = spn_toolchain_artifact_root(support.artifact);
      sp_da(spn_toolchain_target_t) targets = sp_da_new(catalog->mem, spn_toolchain_target_t);
      sp_da_for(declared, it) {
        spn_path_t sdk = declared[it].sdk;
        sp_da_push(targets, ((spn_toolchain_target_t) {
          .triple = declared[it].triple,
          .sdk = spn_path_empty(sdk) ? sdk : spn_path_join(catalog->mem, root, sdk.sub),
        }));
      }
      return targets;
    }
    case SPN_TOOLCHAIN_SUPPORT_LOCAL:
    case SPN_TOOLCHAIN_SUPPORT_NONE: {
      return declared;
    }
  }
  sp_unreachable_return(declared);
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
    .linker = decl->linker,
    .link_args = decl->link_args,
    .targets = sp_da_empty(decl->targets) ? default_targets(catalog, decl) : declared_targets(catalog, decl->targets, support),
    .support = support,
  };
}

void spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, spn_triple_t host, spn_sdk_host_t sdks, sp_mem_t mem) {
  catalog->mem = mem;
  catalog->host = host;
  catalog->sdks = sdks;
  sp_str_om_init(catalog->entries);
}

spn_err_t spn_toolchain_catalog_load(spn_toolchain_catalog_t* catalog, sp_str_t json) {
  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  spn_try(spn_toolchain_decls_parse(catalog->mem, json, &decls));
  sp_da_for(decls, it) {
    spn_toolchain_catalog_add(catalog, decls[it]);
  }
  return SPN_OK;
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
