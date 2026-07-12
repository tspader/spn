#include "toolchain/catalog.h"

#include "toolchains.gen.h"
#include "triple/triple.h"

SP_PRIVATE spn_toolchain_launcher_t spn_toolchain_catalog_load_launcher(const spn_cg_launcher_t* in) {
  return (spn_toolchain_launcher_t) {
    .program = in->program,
    .args = in->args,
  };
}

spn_err_t spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, sp_str_t builtins_json, sp_mem_t mem) {
  catalog->mem = mem;
  sp_str_ht_init(mem, catalog->entries);

  spn_cg_toolchains_t root = SP_ZERO_INITIALIZE();
  if (!spn_toolchains_read(builtins_json, &root, mem)) {
    return SPN_ERROR;
  }

  sp_om_for(root.toolchain, it) {
    const spn_cg_toolchain_t* t = sp_om_at(root.toolchain, it);

    spn_toolchain_info_t toolchain = SP_ZERO_INITIALIZE();
    toolchain.name = t->name;
    toolchain.version = t->version;
    toolchain.driver = t->driver;
    toolchain.compiler = spn_toolchain_catalog_load_launcher(&t->compiler);
    toolchain.cxx = spn_toolchain_catalog_load_launcher(&t->cxx);
    toolchain.linker = spn_toolchain_catalog_load_launcher(&t->linker);
    toolchain.archiver = spn_toolchain_catalog_load_launcher(&t->archiver);

    toolchain.hosts = sp_da_new(mem, spn_toolchain_host_t);
    sp_da_for(t->host, it) {
      sp_da_push(toolchain.hosts, ((spn_toolchain_host_t) {
        .triple = spn_triple_from_str(t->host[it].key),
        .artifact = {
          .url = t->host[it].value.url,
          .sha256 = t->host[it].value.sha256,
          .mirror_list = t->mirrors,
        },
      }));
    }
    toolchain.source = sp_da_empty(toolchain.hosts) ? SPN_TOOLCHAIN_SOURCE_LOCAL : SPN_TOOLCHAIN_SOURCE_DISTRIBUTION;

    toolchain.targets = sp_da_new(mem, spn_triple_t);
    sp_da_for(t->target, it) {
      sp_da_push(toolchain.targets, ((spn_triple_t) {
        .arch = sp_opt_is_null(t->target[it].arch) ? SPN_ARCH_NONE : sp_opt_get(t->target[it].arch),
        .os = sp_opt_is_null(t->target[it].os) ? SPN_OS_NONE : sp_opt_get(t->target[it].os),
        .abi = sp_opt_is_null(t->target[it].abi) ? SPN_ABI_NONE : sp_opt_get(t->target[it].abi),
      }));
    }

    spn_toolchain_catalog_add(catalog, toolchain);
  }

  return SPN_OK;
}

void spn_toolchain_catalog_add(spn_toolchain_catalog_t* catalog, spn_toolchain_info_t toolchain) {
  spn_toolchain_info_t* entry = sp_alloc_type(catalog->mem, spn_toolchain_info_t);
  *entry = toolchain;
  sp_str_ht_insert(catalog->entries, entry->name, entry);
}

spn_toolchain_info_t* spn_toolchain_catalog_get(spn_toolchain_catalog_t* catalog, sp_str_t name) {
  spn_toolchain_info_t** entry = sp_str_ht_get(catalog->entries, name);
  return entry ? *entry : SP_NULLPTR;
}

spn_opt_artifact_t spn_toolchain_select_artifact(sp_da(spn_toolchain_host_t) hosts, spn_triple_t host) {
  spn_opt_artifact_t result = SP_ZERO_INITIALIZE();

  sp_da_for(hosts, it) {
    if (spn_triple_match(hosts[it].triple, host)) {
      sp_opt_set(result, hosts[it].artifact);
      return result;
    }
  }

  return result;
}
