#include "toolchain/catalog.h"

#include "paths/paths.h"
#include "toolchains.gen.h"
#include "triple/triple.h"

SP_PRIVATE spn_toolchain_launcher_t spn_toolchain_catalog_load_launcher(const spn_cg_launcher_t* in) {
  return (spn_toolchain_launcher_t) {
    .program = spn_arg_lit(in->program),
    .args = in->args,
  };
}

spn_err_t spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, sp_str_t builtins_json, sp_mem_t mem) {
  sp_str_om_init(catalog->entries);

  spn_cg_toolchains_t root = sp_zero;
  if (!spn_toolchains_read(builtins_json, &root, mem)) {
    return SPN_ERROR;
  }

  sp_om_for(root.toolchain, it) {
    const spn_cg_toolchain_t* t = sp_om_at(root.toolchain, it);

    spn_toolchain_info_t toolchain = sp_zero;
    toolchain.name = t->name;
    toolchain.version = t->version;
    toolchain.driver = t->driver;
    toolchain.compiler = spn_toolchain_catalog_load_launcher(&t->compiler);
    toolchain.cxx = spn_toolchain_catalog_load_launcher(&t->cxx);
    toolchain.linker = spn_toolchain_catalog_load_launcher(&t->linker);
    toolchain.archiver = spn_toolchain_catalog_load_launcher(&t->archiver);

    toolchain.hosts = sp_da_new(mem, spn_toolchain_host_t);
    sp_da_for(t->host, it) {
      spn_triple_t host_triple = sp_zero;
      if (spn_triple_parse(t->host[it].key, &host_triple) || !host_triple.arch || !host_triple.os) {
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
    toolchain.source = SPN_TOOLCHAIN_SOURCE_LOCAL;
    sp_da_for(toolchain.hosts, at) {
      if (!sp_str_empty(toolchain.hosts[at].artifact.url)) {
        toolchain.source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION;
        break;
      }
    }

    toolchain.targets = sp_da_new(mem, spn_triple_t);
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
