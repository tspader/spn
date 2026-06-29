#include "toolchain/toolchain.h"

#include "intern/intern.h"
#include "triple/triple.h"
#include "toolchains.gen.h"
#include "spn.embed.h"

SP_PRIVATE spn_triple_t load_triple(const spn_cg_triple_t* t) {
  return (spn_triple_t) {
    .arch = sp_opt_is_null(t->arch) ? SPN_ARCH_NONE : sp_opt_get(t->arch),
    .os = sp_opt_is_null(t->os) ? SPN_OS_NONE : sp_opt_get(t->os),
    .abi = sp_opt_is_null(t->abi) ? SPN_ABI_NONE : sp_opt_get(t->abi),
  };
}

SP_PRIVATE spn_toolchain_launcher_t load_launcher(const spn_cg_launcher_t* in) {
  return (spn_toolchain_launcher_t) {
    .program = in->program,
    .args = in->args,
  };
}

SP_PRIVATE spn_cg_download_t find_host(const spn_cg_toolchain_t* t, spn_triple_t host) {
  if (sp_da_empty(t->host)) return (spn_cg_download_t) SP_ZERO_INITIALIZE();

  sp_da_for(t->host, it) {
    if (spn_triple_match(spn_triple_from_str(t->host[it].key), host)) {
      return t->host[it].value;
    }
  }
  return t->host[0].value;
}

sp_da(spn_toolchain_entry_t) spn_toolchain_load_builtins(spn_triple_t host, sp_mem_t mem) {
  sp_da(spn_toolchain_entry_t) out = SP_NULLPTR;
  sp_str_t json = sp_str((const c8*)toolchains_json, toolchains_json_size);

  spn_cg_toolchains_t root = sp_zero;
  if (!spn_toolchains_read(json, &root, mem)) return out;

  sp_om_for(root.toolchain, it) {
    const spn_cg_toolchain_t* t = sp_om_at(root.toolchain, it);
    spn_cg_download_t download = find_host(t, host);

    spn_toolchain_entry_t entry = SP_ZERO_INITIALIZE();
    entry.name = spn_intern(t->name);
    entry.kind = SPN_TOOLCHAIN_INLINE;
    entry.info.name = entry.name;
    entry.info.version = t->version;
    entry.info.url = download.url;
    entry.info.sha = download.sha256;
    entry.info.driver = t->driver;
    entry.info.sysroot = t->sysroot;
    entry.info.compiler = load_launcher(&t->compiler);
    entry.info.linker = load_launcher(&t->linker);
    entry.info.archiver = load_launcher(&t->archiver);

    for (u32 i = 0; i < sp_da_size(t->host) && i < SPN_TOOLCHAIN_MAX_HOSTS; i++) {
      entry.info.hosts[i] = spn_triple_from_str(t->host[i].key);
    }
    for (u32 i = 0; i < sp_da_size(t->target) && i < SPN_TOOLCHAIN_MAX_TARGETS; i++) {
      entry.info.targets[i] = load_triple(&t->target[i]);
    }

    sp_da_push(out, entry);
  }

  return out;
}
