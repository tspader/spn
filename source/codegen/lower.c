#include "codegen/lower.h"

#include "types.gen.h"
#include "enum/enum.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "target/types.h"
#include "toolchain/types.h"
#include "profile/types.h"
#include "index/types.h"

static sp_str_t lower_qualify(spn_codegen_ctx_t* ctx, sp_str_t namespace, sp_str_t name) {
  if (sp_str_empty(name)) return name;
  sp_str_t ns = sp_str_empty(namespace) ? sp_str_lit("core") : namespace;
  return spn_codegen_intern(ctx, sp_str_join(ctx->mem, ns, name, sp_str_lit("/")));
}

static sp_str_t lower_canonicalize(spn_codegen_ctx_t* ctx, sp_str_t name) {
  if (sp_str_empty(name)) return name;
  sp_str_pair_t pair = sp_str_cleave_c8(name, '/');
  if (sp_str_empty(pair.second)) return lower_qualify(ctx, sp_str_lit("core"), name);
  return lower_qualify(ctx, pair.first, pair.second);
}

static spn_toolchain_launcher_t lower_launcher(spn_codegen_ctx_t* ctx, sp_str_t str) {
  spn_toolchain_launcher_t launcher = sp_zero;
  if (sp_str_empty(str)) return launcher;

  if (sp_str_contains(str, sp_str_lit(" "))) {
    sp_da(sp_str_t) parts = sp_str_split_c8(ctx->mem, str, ' ');
    launcher.program = spn_codegen_intern(ctx, parts[0]);
    launcher.args = sp_da_new(ctx->mem, sp_str_t);
    for (u32 i = 1; i < sp_da_size(parts); i++) {
      sp_da_push(launcher.args, spn_codegen_intern(ctx, parts[i]));
    }
  } else {
    launcher.program = spn_codegen_intern(ctx, str);
  }

  return launcher;
}

static spn_triple_t lower_triple(const spn_cg_triple_t* triple) {
  return (spn_triple_t) {
    .arch = sp_opt_is_null(triple->arch) ? SPN_ARCH_NONE : sp_opt_get(triple->arch),
    .os   = sp_opt_is_null(triple->os)   ? SPN_OS_NONE   : sp_opt_get(triple->os),
    .abi  = sp_opt_is_null(triple->abi)  ? SPN_ABI_NONE  : sp_opt_get(triple->abi),
  };
}

static spn_linkage_set_t lower_linkages(sp_da(sp_str_t) kinds) {
  spn_linkage_set_t set = sp_zero;
  sp_da_for(kinds, k) {
    switch (spn_linkage_from_str(kinds[k])) {
      case SPN_LIB_KIND_SOURCE: set.source = true; break;
      case SPN_LIB_KIND_SHARED: set.shared = true; break;
      case SPN_LIB_KIND_STATIC: set.static_lib = true; break;
      case SPN_LIB_KIND_OBJECT: set.object = true; break;
      case SPN_LIB_KIND_NONE: break;
    }
  }
  return set;
}

static bool is_path_absolute(sp_str_t path) {
  return path.len && (path.data[0] == '/' || (path.len >= 2 && path.data[1] == ':'));
}

static spn_target_info_t lower_target(const spn_cg_target_t* cg, spn_target_kind_t kind) {
  return (spn_target_info_t) {
    .name = cg->name,
    .kind = kind,
    .linkages = lower_linkages(cg->kinds),
    .no_link = sp_opt_is_null(cg->link) ? false : !sp_opt_get(cg->link),
    .source = cg->source,
    .headers = cg->headers,
    .include = cg->include,
    .define = cg->define,
    .flags = cg->flags,
    .deps = cg->deps,
  };
}

static void lower_collection(spn_cg_target_om_t cg, spn_target_info_om_t* out, spn_target_kind_t kind) {
  sp_str_om_init(*out);
  sp_om_for(cg, it) {
    spn_target_info_t target = lower_target(sp_str_om_at(cg, it), kind);
    sp_str_om_insert(*out, target.name, target);
  }
}

static void lower_dep(spn_codegen_ctx_t* ctx, sp_str_t name, sp_str_t version, spn_dep_kind_t kind, spn_pkg_info_t* out) {
  spn_requested_pkg_t req = {
    .qualified = lower_canonicalize(ctx, name),
    .kind = kind,
  };

  sp_str_t prefix = sp_str_lit("file://");
  if (sp_str_starts_with(version, prefix)) {
    sp_str_t path = sp_str_strip_left(version, prefix);
    if (!is_path_absolute(path)) {
      path = sp_fs_join_path(ctx->mem, ctx->dir, path);
    }
    req.source = SPN_PKG_SOURCE_FILE;
    req.file.path = sp_fs_normalize_path(ctx->mem, path);
  } else {
    req.source = SPN_PKG_SOURCE_INDEX;
    req.index.range = spn_semver_parse_range(version);
  }

  sp_ht_insert(out->deps, req.qualified, req);
}

static void lower_package(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  const spn_cg_package_t* p = &cg->package;
  out->name = p->name;
  out->namespace = p->namespace;
  out->repo = p->repo;
  out->url = p->url;
  out->author = p->author;
  out->maintainer = p->maintainer;
  out->qualified = lower_qualify(ctx, p->namespace, p->name);
  out->version = spn_semver_from_str(p->version);
  out->include = sp_da_new(ctx->mem, sp_str_t);
  sp_da_for(p->include, it) {
    sp_da_push(out->include, sp_fs_join_path(ctx->mem, ctx->dir, p->include[it]));
  }
  out->define = p->define;
  out->system_deps = p->system_deps;
  out->build = sp_str_empty(p->build) ? sp_str_lit("build.c") : p->build;
  out->configure = sp_str_empty(p->configure) ? sp_str_lit("configure.c") : p->configure;
}

static void lower_versions(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  out->versions = sp_da_new(ctx->mem, spn_semver_t);
  sp_ht_init(ctx->mem, out->metadata);

  spn_semver_t version = spn_semver_from_str(cg->package.version);
  sp_da_push(out->versions, version);
  sp_ht_insert(out->metadata, version, ((spn_pkg_metadata_t) { version, cg->package.commit }));
}

static void lower_targets(const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  lower_collection(cg->lib, &out->libs, SPN_TARGET_LIB);
  lower_collection(cg->bin, &out->exes, SPN_TARGET_EXE);
  lower_collection(cg->script, &out->scripts, SPN_TARGET_SCRIPT);
  lower_collection(cg->test, &out->tests, SPN_TARGET_TEST);
}

static void lower_toolchains(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  sp_str_om_init(out->toolchains);
  sp_da_for(cg->toolchain, n) {
    const spn_cg_manifest_toolchain_t* t = &cg->toolchain[n];

    if (!sp_str_empty(t->package)) continue;

    spn_toolchain_entry_t entry = sp_zero;
    entry.name = t->name;
    entry.kind = sp_str_empty(t->url) ? SPN_TOOLCHAIN_SYSTEM : SPN_TOOLCHAIN_REMOTE;
    entry.info.name = t->name;
    entry.info.url = t->url;
    entry.info.compiler = lower_launcher(ctx, t->compiler);
    entry.info.linker = lower_launcher(ctx, t->linker);
    entry.info.archiver = lower_launcher(ctx, t->archiver);
    entry.info.sysroot = t->sysroot;
    entry.info.driver = sp_opt_is_null(t->driver) ? SPN_CC_DRIVER_NONE : sp_opt_get(t->driver);
    entry.info.export = sp_opt_is_null(t->export) ? false : sp_opt_get(t->export);
    for (u32 i = 0; i < sp_da_size(t->host) && i < SPN_TOOLCHAIN_MAX_HOSTS; i++) {
      entry.info.hosts[i] = lower_triple(&t->host[i]);
    }
    for (u32 i = 0; i < sp_da_size(t->target) && i < SPN_TOOLCHAIN_MAX_TARGETS; i++) {
      entry.info.targets[i] = lower_triple(&t->target[i]);
    }

    sp_str_om_insert(out->toolchains, entry.name, entry);
  }
}

static void lower_profiles(const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  sp_str_om_init(out->profiles);
  sp_da_for(cg->profile, i) {
    const spn_cg_profile_t* p = &cg->profile[i].value;
    spn_profile_info_t info = {
      .name = cg->profile[i].key,
      .toolchain = p->toolchain,
      .os = sp_opt_is_null(p->os) ? SPN_OS_NONE : sp_opt_get(p->os),
      .arch = sp_opt_is_null(p->arch) ? SPN_ARCH_NONE : sp_opt_get(p->arch),
      .abi = sp_opt_is_null(p->abi) ? SPN_ABI_NONE : sp_opt_get(p->abi),
      .linkage = sp_opt_is_null(p->linkage) ? SPN_LIB_KIND_NONE : sp_opt_get(p->linkage),
      .standard = sp_opt_is_null(p->standard) ? SPN_C_STANDARD_NONE : sp_opt_get(p->standard),
      .mode = sp_opt_is_null(p->mode) ? SPN_BUILD_MODE_NONE : sp_opt_get(p->mode),
    };
    sp_str_om_insert(out->profiles, info.name, info);
  }
}

static void lower_indexes(const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  sp_str_om_init(out->indexes);
  sp_om_for(cg->index, it) {
    const spn_cg_index_t* idx = sp_str_om_at(cg->index, it);
    spn_index_info_t info = {
      .name = idx->name,
      .url = idx->url,
      .protocol = idx->protocol,
      .kind = SPN_INDEX_WORKSPACE,
    };
    sp_str_om_insert(out->indexes, info.name, info);
  }
}

static void lower_deps(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  sp_str_ht_init(ctx->mem, out->deps);
  sp_da_for(cg->deps.package, i) {
    lower_dep(ctx, cg->deps.package[i].key, cg->deps.package[i].value, SPN_DEP_KIND_PACKAGE, out);
  }
  sp_da_for(cg->deps.test, i) {
    lower_dep(ctx, cg->deps.test[i].key, cg->deps.test[i].value, SPN_DEP_KIND_TEST, out);
  }
  sp_da_for(cg->deps.build, i) {
    lower_dep(ctx, cg->deps.build[i].key, cg->deps.build[i].value, SPN_DEP_KIND_BUILD, out);
  }
}

static void lower_config(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  out->config = sp_da_new(ctx->mem, spn_pkg_config_entry_t);
  sp_da_for(cg->config, i) {
    spn_pkg_config_entry_t entry = { .key = cg->config[i].key };
    if (!sp_opt_is_null(cg->config[i].value.kind)) {
      sp_opt_set(entry.value.kind, sp_opt_get(cg->config[i].value.kind));
    }
    sp_da_push(out->config, entry);
  }
}

static void validate_lib_linkages(spn_codegen_ctx_t* ctx, spn_pkg_info_t* out) {
  spn_codegen_push_key(ctx, "lib");
  sp_om_for(out->libs, it) {
    spn_linkage_set_t set = sp_str_om_at(out->libs, it)->linkages;
    if (set.object && (set.source || set.shared || set.static_lib)) {
      spn_codegen_push_index(ctx, it);
      spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, "kinds");
      spn_codegen_pop(ctx);
    }
  }
  spn_codegen_pop(ctx);
}

static void validate_collection_links(spn_codegen_ctx_t* ctx, spn_cg_target_om_t cg, const c8* key) {
  spn_codegen_push_key(ctx, key);
  sp_om_for(cg, it) {
    if (!sp_opt_is_null(sp_str_om_at(cg, it)->link)) {
      spn_codegen_push_index(ctx, it);
      spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, "link");
      spn_codegen_pop(ctx);
    }
  }
  spn_codegen_pop(ctx);
}

static void validate_links(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg) {
  validate_collection_links(ctx, cg->bin, "bin");
  validate_collection_links(ctx, cg->script, "script");
  validate_collection_links(ctx, cg->test, "test");
}

static void validate_unique_targets(spn_codegen_ctx_t* ctx, spn_pkg_info_t* out) {
  sp_ht(sp_str_t, u8) seen;
  sp_str_ht_init(ctx->mem, seen);

  struct {
    const c8* key;
    spn_target_info_om_t om;
  } groups[] = {
    { "lib", out->libs },
    { "bin", out->exes },
    { "script", out->scripts },
    { "test", out->tests },
  };

  sp_for(g, SP_CARR_LEN(groups)) {
    spn_codegen_push_key(ctx, groups[g].key);
    sp_om_for(groups[g].om, it) {
      sp_str_t name = sp_str_om_at(groups[g].om, it)->name;
      if (sp_ht_getp(seen, name)) {
        spn_codegen_push_index(ctx, it);
        spn_codegen_issue_at(ctx, SPN_CODEGEN_ERR_DUPLICATE_KEY, name);
        spn_codegen_pop(ctx);
      } else {
        sp_ht_insert(seen, name, 1);
      }
    }
    spn_codegen_pop(ctx);
  }
}

static void validate_inline_toolchains(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg) {
  spn_codegen_push_key(ctx, "toolchain");
  sp_da_for(cg->toolchain, it) {
    const spn_cg_manifest_toolchain_t* t = &cg->toolchain[it];
    if (!sp_str_empty(t->package)) {
      continue;
    }

    spn_codegen_push_index(ctx, it);
    if (sp_str_empty(t->name))     { spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "name"); }
    if (sp_str_empty(t->compiler)) { spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "compiler"); }
    if (sp_str_empty(t->linker))   { spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "linker"); }
    if (sp_str_empty(t->archiver)) { spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "archiver"); }
    if (sp_opt_is_null(t->driver) || sp_opt_get(t->driver) == SPN_CC_DRIVER_NONE) {
      spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "driver");
    }
    if (!sp_da_size(t->host))   { spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "host"); }
    if (!sp_da_size(t->target)) { spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "target"); }
    spn_codegen_pop(ctx);
  }
  spn_codegen_pop(ctx);
}

spn_err_t spn_pkg_lower(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  out->arena = sp_mem_arena_new(ctx->mem);

  lower_package(ctx, cg, out);
  lower_versions(ctx, cg, out);
  lower_targets(cg, out);
  lower_toolchains(ctx, cg, out);
  lower_profiles(cg, out);
  lower_indexes(cg, out);
  lower_deps(ctx, cg, out);
  lower_config(ctx, cg, out);

  validate_lib_linkages(ctx, out);
  validate_links(ctx, cg);
  validate_unique_targets(ctx, out);
  validate_inline_toolchains(ctx, cg);

  return sp_da_empty(ctx->issues) ? SPN_OK : SPN_ERROR;
}

spn_err_t spn_codegen_load_pkg(spn_codegen_ctx_t* ctx, sp_str_t manifest, spn_pkg_info_t* out) {
  ctx->dir = sp_fs_parent_path(manifest);

  spn_cg_manifest_t cg = sp_zero;
  spn_err_t err = spn_codegen_load(ctx, manifest, &cg);
  if (err) {
    return err;
  }

  return spn_pkg_lower(ctx, &cg, out);
}
