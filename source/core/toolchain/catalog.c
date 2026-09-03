#include "toolchain/catalog.h"

#include "paths/paths.h"
#include "toolchain/toolchain.h"
#include "toolchains.gen.h"
#include "triple/triple.h"

static spn_toolchain_launcher_t load_launcher(const spn_cg_launcher_t* in) {
  return (spn_toolchain_launcher_t) {
    .program = spn_arg_lit(in->program),
    .args = in->args,
  };
}

static sp_da(spn_triple_t) bind_targets(spn_toolchain_catalog_t* catalog, const spn_toolchain_info_t* toolchain) {
  if (!sp_da_empty(toolchain->targets)) {
    return toolchain->targets;
  }

  sp_da(spn_triple_t) targets = sp_da_new(catalog->mem, spn_triple_t);
  sp_da_push(targets, catalog->host);
  bool elf = catalog->host.os == SPN_OS_LINUX;
  bool bare_metal = spn_toolchain_driver_caps(toolchain->driver) & SPN_CC_CAP_FREESTANDING;
  if (elf && bare_metal) {
    sp_da_push(targets, ((spn_triple_t) { catalog->host.arch, SPN_OS_FREESTANDING, SPN_ABI_BARE }));
  }
  return targets;
}

static bool has_host(sp_da(spn_toolchain_host_t) hosts, spn_triple_t host) {
  sp_da_for(hosts, it) {
    if (spn_triple_match(hosts[it].triple, host)) {
      return true;
    }
  }
  return false;
}

static spn_toolchain_support_t bind_support(spn_toolchain_catalog_t* catalog, const spn_toolchain_info_t* toolchain) {
  sp_da(spn_toolchain_host_t) hosts = toolchain->hosts;
  switch (toolchain->source) {
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

void spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, spn_triple_t host, sp_mem_t mem) {
  catalog->mem = mem;
  catalog->host = host;
  sp_str_om_init(catalog->entries);
}

spn_err_t spn_toolchain_catalog_load(spn_toolchain_catalog_t* catalog, sp_str_t json) {
  spn_cg_toolchains_t root = sp_zero;
  if (!spn_toolchains_read(json, &root, catalog->mem)) {
    return SPN_ERROR;
  }

  sp_om_for(root.toolchain, it) {
    const spn_cg_toolchain_t* t = sp_om_at(root.toolchain, it);

    spn_toolchain_info_t toolchain = sp_zero;
    toolchain.name = t->name;
    toolchain.version = t->version;
    toolchain.driver = t->driver;
    toolchain.compiler = load_launcher(&t->compiler);
    toolchain.cxx = load_launcher(&t->cxx);
    toolchain.linker = load_launcher(&t->linker);
    toolchain.archiver = load_launcher(&t->archiver);

    toolchain.hosts = sp_da_new(catalog->mem, spn_toolchain_host_t);
    sp_da_for(t->host, it) {
      spn_triple_t host_triple = sp_zero;
      if (spn_triple_parse_host(t->host[it].key, &host_triple)) {
        return SPN_ERROR;
      }
      sp_da_push(toolchain.hosts, ((spn_toolchain_host_t) {
        .triple = host_triple,
        .artifact = {
          .url = t->host[it].value.url,
          .sha256 = t->host[it].value.sha256,
          .mirror_list = t->mirrors,
        },
      }));
    }
    toolchain.source = spn_toolchain_source(toolchain.hosts);
    if (toolchain.source == SPN_TOOLCHAIN_SOURCE_MIXED) {
      return SPN_ERROR;
    }

    toolchain.targets = sp_da_new(catalog->mem, spn_triple_t);
    sp_da_for(t->target, it) {
      spn_triple_t partial = {
        .arch = sp_opt_is_null(t->target[it].arch) ? SPN_ARCH_NONE : sp_opt_get(t->target[it].arch),
        .os = sp_opt_is_null(t->target[it].os) ? SPN_OS_NONE : sp_opt_get(t->target[it].os),
        .abi = sp_opt_is_null(t->target[it].abi) ? SPN_ABI_NONE : sp_opt_get(t->target[it].abi),
      };
      spn_triple_t full = sp_zero;
      if (spn_triple_entry(partial, &full) != SPN_TRIPLE_ENTRY_OK) {
        return SPN_ERROR;
      }
      sp_da_push(toolchain.targets, full);
    }

    spn_toolchain_catalog_add(catalog, toolchain);
  }

  return SPN_OK;
}

void spn_toolchain_catalog_add(spn_toolchain_catalog_t* catalog, spn_toolchain_info_t toolchain) {
  toolchain.targets = bind_targets(catalog, &toolchain);
  toolchain.support = bind_support(catalog, &toolchain);

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
