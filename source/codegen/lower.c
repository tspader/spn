#include "codegen/lower.h"

#include "manifest.gen.h"
#include "enum/enum.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "target/types.h"
#include "target/mutate.h"
#include "toolchain/catalog.h"
#include "toolchain/types.h"
#include "triple/triple.h"
#include "profile/types.h"
#include "index/types.h"
#include "toml/loader.h"
#include "when/when.h"

static sp_str_t lower_qualify(spn_toml_loader_t* ctx, sp_str_t namespace, sp_str_t name) {
  if (sp_str_empty(name)) return name;
  sp_str_t ns = sp_str_empty(namespace) ? sp_str_lit("core") : namespace;
  return spn_toml_loader_intern(ctx, sp_str_join(ctx->mem, ns, name, sp_str_lit("/")));
}

static sp_str_t lower_canonicalize(spn_toml_loader_t* ctx, sp_str_t name) {
  if (sp_str_empty(name)) return name;
  sp_str_pair_t pair = sp_str_cleave_c8(name, '/');
  if (sp_str_empty(pair.second)) return lower_qualify(ctx, sp_str_lit("core"), name);
  return lower_qualify(ctx, pair.first, pair.second);
}

static spn_toolchain_launcher_t lower_launcher(spn_toml_loader_t* ctx, sp_str_t str) {
  spn_toolchain_launcher_t launcher = sp_zero;
  if (sp_str_empty(str)) return launcher;

  if (sp_str_contains(str, sp_str_lit(" "))) {
    sp_da(sp_str_t) parts = sp_str_split_c8(ctx->mem, str, ' ');
    launcher.args = sp_da_new(ctx->mem, sp_str_t);
    for (u32 i = 0; i < sp_da_size(parts); i++) {
      if (sp_str_empty(parts[i])) continue;
      if (sp_str_empty(launcher.program)) {
        launcher.program = spn_toml_loader_intern(ctx, parts[i]);
        continue;
      }
      sp_da_push(launcher.args, spn_toml_loader_intern(ctx, parts[i]));
    }
  } else {
    launcher.program = spn_toml_loader_intern(ctx, str);
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

static spn_cxx_options_t lower_cxx_options(const spn_cg_cxx_options_t* cg) {
  return (spn_cxx_options_t) {
    .standard = sp_opt_is_null(cg->standard) ? SPN_CXX_STANDARD_NONE : sp_opt_get(cg->standard),
    .no_exceptions = sp_opt_is_null(cg->exceptions) ? false : !sp_opt_get(cg->exceptions),
    .no_rtti = sp_opt_is_null(cg->rtti) ? false : !sp_opt_get(cg->rtti),
  };
}

static spn_gated_list_t lower_gated_sources(spn_toml_loader_t* ctx, sp_da(spn_cg_source_entry_t) entries) {
  spn_gated_list_t values = sp_da_new(ctx->mem, spn_gated_str_t);
  sp_da_for(entries, it) {
    sp_da_push(values, ((spn_gated_str_t) { .value = entries[it].path, .when = entries[it].when }));
  }
  return values;
}

static spn_gated_list_t lower_gated_values(spn_toml_loader_t* ctx, sp_da(spn_cg_value_entry_t) entries) {
  spn_gated_list_t values = sp_da_new(ctx->mem, spn_gated_str_t);
  sp_da_for(entries, it) {
    sp_da_push(values, ((spn_gated_str_t) { .value = entries[it].value, .when = entries[it].when }));
  }
  return values;
}

static spn_target_info_t lower_target(spn_toml_loader_t* ctx, const spn_cg_target_t* cg, spn_target_kind_t kind) {
  spn_target_info_t target = {
    .name = cg->name,
    .kind = kind,
    .linkages = lower_linkages(cg->kinds),
    .no_link = sp_opt_is_null(cg->link) ? false : !sp_opt_get(cg->link),
    .headers = cg->headers,
    .include = cg->include,
    .cxx = lower_cxx_options(&cg->cxx),
    .gated = {
      .source = lower_gated_sources(ctx, cg->source),
      .define = lower_gated_values(ctx, cg->define),
      .flags = lower_gated_values(ctx, cg->flags),
      .system_deps = lower_gated_values(ctx, cg->system_deps),
      .deps = sp_da_new(ctx->mem, spn_gated_str_t),
    },
  };
  sp_da_for(cg->deps, it) {
    sp_da_push(target.gated.deps, ((spn_gated_str_t) { .value = cg->deps[it].pkg, .when = cg->deps[it].when }));
  }
  spn_target_info_init(ctx->mem, &target);
  return target;
}

static void lower_collection(spn_toml_loader_t* ctx, spn_cg_target_om_t cg, spn_target_map_t* out, spn_target_kind_t kind) {
  sp_str_om_init(*out);
  sp_om_for(cg, it) {
    spn_target_info_t target = lower_target(ctx, sp_str_om_at(cg, it), kind);
    sp_str_om_insert(*out, target.name, target);
  }
}

static spn_target_info_t lower_metaprogram(spn_toml_loader_t* ctx, const spn_cg_build_script_t* cg, sp_str_t name, spn_target_kind_t kind) {
  spn_target_info_t program = {
    .name = name,
    .kind = kind,
    .source = cg->source,
    .include = cg->include,
    .define = cg->define,
    .flags = cg->flags,
  };
  spn_target_info_init(ctx->mem, &program);
  return program;
}

static void lower_dep(spn_toml_loader_t* ctx, sp_str_t name, const spn_cg_dep_t* cg, spn_dep_kind_t kind, spn_pkg_info_t* out) {
  spn_requested_dep_t req = {
    .qualified = lower_canonicalize(ctx, name),
    .kind = kind,
    .private = sp_opt_is_null(cg->private) ? false : sp_opt_get(cg->private),
    .when = cg->when,
    .options = cg->options,
  };

  if (!sp_str_empty(cg->path)) {
    if (!sp_str_empty(cg->version)) {
      spn_toml_loader_push_key(ctx, sp_str_to_cstr(ctx->mem, name));
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "path");
      spn_toml_loader_pop(ctx);
      return;
    }
    sp_str_t path = cg->path;
    if (!is_path_absolute(path)) {
      path = sp_fs_join_path(ctx->mem, ctx->dir, path);
    }
    req.source = SPN_PKG_SOURCE_FILE;
    req.file.path = sp_fs_normalize_path(ctx->mem, sp_fs_join_path(ctx->mem, path, sp_str_lit("spn.toml")));
  } else {
    req.source = SPN_PKG_SOURCE_INDEX;
    if (sp_str_empty(cg->version) || spn_semver_parse_range(cg->version, &req.index.range)) {
      spn_toml_loader_push_key(ctx, sp_str_to_cstr(ctx->mem, name));
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "version");
      spn_toml_loader_pop(ctx);
      return;
    }
  }

  sp_da_push(out->deps, req);
}

static void lower_package(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* info) {
  const spn_cg_package_t* p = &cg->package;
  info->name = p->name;
  info->namespace = p->namespace;
  info->repo = p->repo;
  info->upstream.url = p->url;
  info->upstream.commit = p->commit;
  info->author = p->author;
  info->maintainer = p->maintainer;
  info->qualified = lower_qualify(ctx, p->namespace, p->name);
  info->version = spn_semver_from_str(p->version);
  info->include = sp_da_new(ctx->mem, sp_str_t);
  sp_da_for(p->include, it) {
    sp_da_push(info->include, sp_fs_join_path(ctx->mem, ctx->dir, p->include[it]));
  }
  info->define = p->define ? p->define : sp_da_new(ctx->mem, sp_str_t);
  info->public_define = sp_da_new(ctx->mem, sp_str_t);
  info->system_deps = sp_da_new(ctx->mem, sp_str_t);
  info->gated.system_deps = sp_da_new(ctx->mem, spn_gated_str_t);
  sp_da_for(p->system_deps, it) {
    sp_da_push(info->gated.system_deps, ((spn_gated_str_t) { .value = p->system_deps[it].lib, .when = p->system_deps[it].when }));
  }
  info->build = lower_metaprogram(ctx, &p->build, sp_str_lit("build"), SPN_TARGET_BUILD_METAPROGRAM);
  info->configure = lower_metaprogram(ctx, &p->configure, sp_str_lit("configure"), SPN_TARGET_CONFIGURE_METAPROGRAM);
}

static bool publish_mount_ok(sp_str_t path) {
  sp_str_t mount = sp_str_cleave_c8(path, '/').first;
  const c8* mounts [] = { "store", "include", "lib", "vendor", "source", "work", "project" };
  sp_carr_for(mounts, it) {
    if (sp_str_equal_cstr(mount, mounts[it])) return true;
  }
  return false;
}

static void lower_publish(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, "publish");
  out->publish.copy = sp_da_new(ctx->mem, spn_publish_copy_t);
  sp_da_for(cg->publish.copy, it) {
    spn_publish_copy_t copy = {
      .from = cg->publish.copy[it].from,
      .to = cg->publish.copy[it].to,
    };
    if (!publish_mount_ok(copy.from) || !publish_mount_ok(copy.to)) {
      spn_toml_loader_push_index(ctx, it);
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "copy");
      spn_toml_loader_pop(ctx);
      continue;
    }
    sp_da_push(out->publish.copy, copy);
  }
  spn_toml_loader_pop(ctx);
}

static void lower_targets(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  lower_collection(ctx, cg->lib, &out->libs, SPN_TARGET_LIB);
  lower_collection(ctx, cg->bin, &out->exes, SPN_TARGET_EXE);
  lower_collection(ctx, cg->script, &out->scripts, SPN_TARGET_SCRIPT);
  lower_collection(ctx, cg->test, &out->tests, SPN_TARGET_TEST);
}

static void lower_toolchains(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  sp_str_om_init(out->toolchains);
  sp_da_for(cg->toolchain, n) {
    const spn_cg_manifest_toolchain_t* t = &cg->toolchain[n];

    spn_toolchain_t toolchain = sp_zero;
    toolchain.name = t->name;
    toolchain.driver = sp_opt_is_null(t->driver) ? SPN_CC_DRIVER_NONE : sp_opt_get(t->driver);
    toolchain.compiler = lower_launcher(ctx, t->compiler);
    toolchain.cxx = lower_launcher(ctx, t->cxx);
    toolchain.linker = lower_launcher(ctx, t->linker);
    toolchain.archiver = lower_launcher(ctx, t->archiver);

    sp_da(spn_toolchain_host_t) hosts = sp_da_new(ctx->mem, spn_toolchain_host_t);
    sp_da_for(t->host, i) {
      sp_da_push(hosts, ((spn_toolchain_host_t) {
        .triple = spn_triple_from_str(t->host[i].key),
        .artifact = {
          .url = t->host[i].value.url,
          .sha256 = t->host[i].value.sha256,
          .mirror_list = t->mirrors,
        },
      }));
    }
    toolchain.artifact = spn_toolchain_select_artifact(hosts, spn_triple_host());

    toolchain.targets = sp_da_new(ctx->mem, spn_triple_t);
    sp_da_for(t->target, i) {
      sp_da_push(toolchain.targets, lower_triple(&t->target[i]));
    }

    sp_str_om_insert(out->toolchains, toolchain.name, toolchain);
  }
}

static spn_sanitizer_set_t lower_sanitizers(sp_da(spn_sanitizer_t) sanitizers) {
  spn_sanitizer_set_t set = 0;
  sp_da_for(sanitizers, it) {
    set |= sanitizers[it];
  }
  return set;
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
      .opt = sp_opt_is_null(p->opt) ? SPN_OPT_LEVEL_NONE : sp_opt_get(p->opt),
      .sanitizers = lower_sanitizers(p->sanitize),
      .sanitizers_set = p->sanitize != SP_NULLPTR,
      .options = p->options,
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
      .rev = idx->rev,
      .protocol = idx->protocol,
      .kind = SPN_INDEX_WORKSPACE,
    };
    sp_str_om_insert(out->indexes, info.name, info);
  }
}

static void lower_deps(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  out->deps = sp_da_new(ctx->mem, spn_requested_dep_t);
  sp_da_for(cg->deps.package, i) {
    lower_dep(ctx, cg->deps.package[i].key, &cg->deps.package[i].value, SPN_DEP_KIND_PACKAGE, out);
  }
  sp_da_for(cg->deps.test, i) {
    lower_dep(ctx, cg->deps.test[i].key, &cg->deps.test[i].value, SPN_DEP_KIND_TEST, out);
  }
  sp_da_for(cg->deps.build, i) {
    lower_dep(ctx, cg->deps.build[i].key, &cg->deps.build[i].value, SPN_DEP_KIND_BUILD, out);
  }
}

static void lower_options(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  sp_str_om_init(out->options);
  sp_da_for(cg->options, it) {
    const spn_cg_manifest_options_entry_t* entry = &cg->options[it];
    spn_option_info_t option = {
      .name = entry->key,
      .type = entry->value.type,
      .additive = sp_opt_is_null(entry->value.additive) ? false : sp_opt_get(entry->value.additive),
      .public = sp_opt_is_null(entry->value.public) ? false : sp_opt_get(entry->value.public),
      .define = entry->value.define,
      .values = entry->value.values ? entry->value.values : sp_da_new(ctx->mem, sp_str_t),
      .defaults = entry->value.defaults ? entry->value.defaults : sp_da_new(ctx->mem, spn_option_default_t),
    };
    sp_str_om_insert(out->options, option.name, option);
  }
}

static void lower_config(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  out->config = sp_da_new(ctx->mem, spn_pkg_config_entry_t);
  sp_da_for(cg->config, i) {
    spn_pkg_config_entry_t entry = {
      .key = cg->config[i].key,
      .value = {
        .options = cg->config[i].value.options,
        .defaults_declined = !sp_opt_is_null(cg->config[i].value.defaults) && !sp_opt_get(cg->config[i].value.defaults),
      },
    };
    if (!sp_opt_is_null(cg->config[i].value.kind) && sp_opt_get(cg->config[i].value.kind) != SPN_LIB_KIND_NONE) {
      sp_opt_set(entry.value.kind, sp_opt_get(cg->config[i].value.kind));
    }
    sp_da_push(out->config, entry);
  }
}

static bool when_key_is_sanitizer_fact(sp_str_t key) {
  sp_str_t prefix = sp_str_lit("sanitize_");
  if (!sp_str_starts_with(key, prefix)) return false;
  sp_str_t name = sp_str_suffix(key, key.len - prefix.len);
  return spn_sanitizer_from_str(name) != SPN_SANITIZER_NONE;
}

static bool when_key_is_fact(sp_str_t key) {
  return sp_str_equal_cstr(key, "os")
    || sp_str_equal_cstr(key, "arch")
    || sp_str_equal_cstr(key, "abi")
    || sp_str_equal_cstr(key, "mode")
    || sp_str_equal_cstr(key, "opt")
    || when_key_is_sanitizer_fact(key);
}

static bool when_fact_value_valid(sp_str_t key, spn_option_value_t value) {
  if (when_key_is_sanitizer_fact(key)) {
    return value.kind == SPN_OPTION_VALUE_BOOL;
  }
  if (value.kind != SPN_OPTION_VALUE_STR) return false;
  if (sp_str_equal_cstr(key, "os"))   return spn_os_from_str(value.str) != SPN_OS_NONE;
  if (sp_str_equal_cstr(key, "arch")) return spn_arch_from_str(value.str) != SPN_ARCH_NONE;
  if (sp_str_equal_cstr(key, "abi"))  return spn_abi_from_str(value.str) != SPN_ABI_NONE;
  if (sp_str_equal_cstr(key, "opt"))  return spn_opt_level_from_str(value.str) != SPN_OPT_LEVEL_NONE;
  return sp_str_equal(value.str, spn_build_mode_to_str(SPN_BUILD_MODE_RELEASE))
      || sp_str_equal(value.str, spn_build_mode_to_str(SPN_BUILD_MODE_DEBUG));
}

static bool when_option_value_valid(const spn_option_info_t* option, spn_option_value_t value) {
  if (option->type == SPN_OPTION_TYPE_NONE) {
    return true;
  }
  if (option->type == SPN_OPTION_TYPE_ENUM && sp_da_empty(option->values)) {
    return value.kind == SPN_OPTION_VALUE_STR;
  }
  return spn_option_value_ok(option, value);
}

static void validate_when(spn_toml_loader_t* ctx, const spn_when_t* when, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, "when");
  sp_da_for(when->clauses, it) {
    const spn_when_clause_t* clause = &when->clauses[it];
    bool ok = false;
    if (when_key_is_fact(clause->key)) {
      ok = when_fact_value_valid(clause->key, clause->value);
    }
    else {
      spn_option_info_t** option = sp_str_om_getp(out->options, clause->key);
      ok = option && when_option_value_valid(*option, clause->value);
    }
    if (!ok) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, clause->key.data);
    }
  }
  spn_toml_loader_pop(ctx);
}

static const c8* dep_kind_key(spn_dep_kind_t kind) {
  switch (kind) {
    case SPN_DEP_KIND_PACKAGE: return "package";
    case SPN_DEP_KIND_TEST:    return "test";
    case SPN_DEP_KIND_BUILD:   return "build";
  }
  SP_UNREACHABLE_RETURN("package");
}

static void validate_dep_whens(spn_toml_loader_t* ctx, spn_pkg_info_t* out) {
  u32 counters [3] = sp_zero;
  spn_toml_loader_push_key(ctx, "deps");
  sp_da_for(out->deps, it) {
    spn_requested_dep_t* req = &out->deps[it];
    spn_toml_loader_push_key(ctx, dep_kind_key(req->kind));
    spn_toml_loader_push_index(ctx, counters[req->kind]++);
    validate_when(ctx, &req->when, out);
    spn_toml_loader_pop(ctx);
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

static void validate_source_whens(spn_toml_loader_t* ctx, sp_da(spn_cg_source_entry_t) entries, const c8* key, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, key);
  sp_da_for(entries, it) {
    spn_toml_loader_push_index(ctx, it);
    validate_when(ctx, &entries[it].when, out);
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

static void validate_value_whens(spn_toml_loader_t* ctx, sp_da(spn_cg_value_entry_t) entries, const c8* key, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, key);
  sp_da_for(entries, it) {
    spn_toml_loader_push_index(ctx, it);
    validate_when(ctx, &entries[it].when, out);
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

static void validate_target_whens(spn_toml_loader_t* ctx, spn_cg_target_om_t targets, const c8* key, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, key);
  sp_om_for(targets, it) {
    const spn_cg_target_t* target = sp_str_om_at(targets, it);
    spn_toml_loader_push_index(ctx, it);
    validate_source_whens(ctx, target->source, "source", out);
    validate_value_whens(ctx, target->define, "define", out);
    validate_value_whens(ctx, target->flags, "flags", out);
    validate_value_whens(ctx, target->system_deps, "system_deps", out);
    spn_toml_loader_push_key(ctx, "deps");
    sp_da_for(target->deps, jt) {
      spn_toml_loader_push_index(ctx, jt);
      validate_when(ctx, &target->deps[jt].when, out);
      spn_toml_loader_pop(ctx);
    }
    spn_toml_loader_pop(ctx);
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

static void validate_pkg_system_dep_whens(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, "package");
  spn_toml_loader_push_key(ctx, "system_deps");
  sp_da_for(cg->package.system_deps, it) {
    spn_toml_loader_push_index(ctx, it);
    validate_when(ctx, &cg->package.system_deps[it].when, out);
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
  spn_toml_loader_pop(ctx);
}

static void validate_option_set(spn_toml_loader_t* ctx, const spn_when_t* set) {
  spn_toml_loader_push_key(ctx, "options");
  sp_da_for(set->clauses, it) {
    if (set->clauses[it].negated) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, set->clauses[it].key.data);
    }
  }
  spn_toml_loader_pop(ctx);
}

// Profiles set the root's own options, so both names and values check against
// this manifest's declarations; config sets other packages' options, whose
// declarations only exist once those manifests load, so values wait for merge
static void validate_option_sets(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, "config");
  sp_da_for(cg->config, it) {
    spn_toml_loader_push_index(ctx, it);
    validate_option_set(ctx, &cg->config[it].value.options);
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);

  spn_toml_loader_push_key(ctx, "profile");
  sp_da_for(cg->profile, it) {
    spn_toml_loader_push_index(ctx, it);
    validate_option_set(ctx, &cg->profile[it].value.options);
    spn_toml_loader_push_key(ctx, "options");
    sp_da_for(cg->profile[it].value.options.clauses, jt) {
      const spn_when_clause_t* clause = &cg->profile[it].value.options.clauses[jt];
      spn_option_info_t** option = sp_str_om_getp(out->options, clause->key);
      if (!option || !when_option_value_valid(*option, clause->value)) {
        spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, clause->key.data);
      }
    }
    spn_toml_loader_pop(ctx);
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

static void validate_options(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, "options");
  sp_da_for(cg->options, it) {
    const spn_cg_option_t* option = &cg->options[it].value;
    spn_toml_loader_push_index(ctx, it);

    if (when_key_is_fact(cg->options[it].key)) {
      spn_toml_loader_issue_at(ctx, SPN_CODEGEN_ERR_DUPLICATE_KEY, cg->options[it].key);
    }
    if (option->type == SPN_OPTION_TYPE_NONE) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "type");
    }
    if (option->type == SPN_OPTION_TYPE_ENUM && sp_da_empty(option->values)) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "values");
    }
    if (option->type == SPN_OPTION_TYPE_BOOL && !sp_da_empty(option->values)) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "values");
    }
    if (option->type != SPN_OPTION_TYPE_BOOL && !sp_str_empty(option->define)) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "define");
    }
    if (option->type != SPN_OPTION_TYPE_BOOL && !sp_opt_is_null(option->additive)) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "additive");
    }
    if (!sp_opt_is_null(option->public) && sp_str_empty(option->define)) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "public");
    }

    spn_option_info_t** lowered = sp_str_om_getp(out->options, cg->options[it].key);
    spn_toml_loader_push_key(ctx, "default");
    sp_da_for(option->defaults, jt) {
      const spn_option_default_t* entry = &option->defaults[jt];
      spn_toml_loader_push_index(ctx, jt);
      if (lowered && !when_option_value_valid(*lowered, entry->value)) {
        spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "value");
      }
      validate_when(ctx, &entry->when, out);
      spn_toml_loader_pop(ctx);
    }
    spn_toml_loader_pop(ctx);

    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

static void validate_whens(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  validate_dep_whens(ctx, out);
  validate_target_whens(ctx, cg->lib, "lib", out);
  validate_target_whens(ctx, cg->bin, "bin", out);
  validate_target_whens(ctx, cg->script, "script", out);
  validate_target_whens(ctx, cg->test, "test", out);
  validate_pkg_system_dep_whens(ctx, cg, out);
  validate_option_sets(ctx, cg, out);
}

static void validate_profiles(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg) {
  spn_toml_loader_push_key(ctx, "profile");
  sp_da_for(cg->profile, it) {
    const spn_cg_profile_t* p = &cg->profile[it].value;
    spn_toml_loader_push_index(ctx, it);
    if (!sp_opt_is_null(p->opt) && sp_opt_get(p->opt) == SPN_OPT_LEVEL_NONE) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "opt");
    }
    if (spn_sanitizer_set_conflicting(lower_sanitizers(p->sanitize))) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "sanitize");
    }
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

static void validate_lib_linkages(spn_toml_loader_t* ctx, spn_pkg_info_t* out) {
  spn_toml_loader_push_key(ctx, "lib");
  sp_om_for(out->libs, it) {
    spn_linkage_set_t set = sp_str_om_at(out->libs, it)->linkages;
    if (set.object && (set.source || set.shared || set.static_lib)) {
      spn_toml_loader_push_index(ctx, it);
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "kinds");
      spn_toml_loader_pop(ctx);
    }
  }
  spn_toml_loader_pop(ctx);
}

static void validate_collection_links(spn_toml_loader_t* ctx, spn_cg_target_om_t cg, const c8* key) {
  spn_toml_loader_push_key(ctx, key);
  sp_om_for(cg, it) {
    if (!sp_opt_is_null(sp_str_om_at(cg, it)->link)) {
      spn_toml_loader_push_index(ctx, it);
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "link");
      spn_toml_loader_pop(ctx);
    }
  }
  spn_toml_loader_pop(ctx);
}

static void validate_links(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg) {
  validate_collection_links(ctx, cg->bin, "bin");
  validate_collection_links(ctx, cg->script, "script");
  validate_collection_links(ctx, cg->test, "test");
}

static void validate_c_only_sources(spn_toml_loader_t* ctx, sp_da(sp_str_t) source) {
  sp_da_for(source, it) {
    if (spn_lang_from_path(source[it]) == SPN_LANG_CXX) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "source");
    }
  }
}

static void validate_collection_cxx(spn_toml_loader_t* ctx, spn_cg_target_om_t cg, const c8* key) {
  spn_toml_loader_push_key(ctx, key);
  sp_om_for(cg, it) {
    const spn_cg_target_t* target = sp_str_om_at(cg, it);
    if (!sp_opt_is_null(target->cxx.standard) && sp_opt_get(target->cxx.standard) == SPN_CXX_STANDARD_NONE) {
      spn_toml_loader_push_index(ctx, it);
      spn_toml_loader_push_key(ctx, "cxx");
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_INVALID, "standard");
      spn_toml_loader_pop(ctx);
      spn_toml_loader_pop(ctx);
    }
  }
  spn_toml_loader_pop(ctx);
}

static void validate_upstream(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg) {
  const spn_cg_package_t* p = &cg->package;
  if (sp_str_empty(p->url) != sp_str_empty(p->commit)) {
    spn_toml_loader_push_key(ctx, "package");
    spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, sp_str_empty(p->commit) ? "commit" : "url");
    spn_toml_loader_pop(ctx);
  }
}

static void validate_cxx(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg) {
  validate_collection_cxx(ctx, cg->lib, "lib");
  validate_collection_cxx(ctx, cg->bin, "bin");
  validate_collection_cxx(ctx, cg->script, "script");
  validate_collection_cxx(ctx, cg->test, "test");
}

// Build and configure scripts compile to wasm, where C++ would drag libc++
// into every module; they stay C
static void validate_c_only_scripts(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg) {
  spn_toml_loader_push_key(ctx, "package");
  spn_toml_loader_push_key(ctx, "build");
  validate_c_only_sources(ctx, cg->package.build.source);
  spn_toml_loader_pop(ctx);
  spn_toml_loader_push_key(ctx, "configure");
  validate_c_only_sources(ctx, cg->package.configure.source);
  spn_toml_loader_pop(ctx);
  spn_toml_loader_pop(ctx);
}

static void validate_unique_targets(spn_toml_loader_t* ctx, spn_pkg_info_t* out) {
  sp_ht(sp_str_t, u8) seen;
  sp_str_ht_init(ctx->mem, seen);

  struct {
    const c8* key;
    spn_target_map_t om;
  } groups[] = {
    { "lib", out->libs },
    { "bin", out->exes },
    { "script", out->scripts },
    { "test", out->tests },
  };

  sp_for(g, SP_CARR_LEN(groups)) {
    spn_toml_loader_push_key(ctx, groups[g].key);
    sp_om_for(groups[g].om, it) {
      sp_str_t name = sp_str_om_at(groups[g].om, it)->name;
      if (sp_ht_getp(seen, name)) {
        spn_toml_loader_push_index(ctx, it);
        spn_toml_loader_issue_at(ctx, SPN_CODEGEN_ERR_DUPLICATE_KEY, name);
        spn_toml_loader_pop(ctx);
      } else {
        sp_ht_insert(seen, name, 1);
      }
    }
    spn_toml_loader_pop(ctx);
  }
}

static void validate_inline_toolchains(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg) {
  spn_toml_loader_push_key(ctx, "toolchain");
  sp_da_for(cg->toolchain, it) {
    const spn_cg_manifest_toolchain_t* t = &cg->toolchain[it];

    spn_toml_loader_push_index(ctx, it);
    if (sp_str_empty(t->name))     { spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "name"); }
    if (sp_str_empty(t->compiler)) { spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "compiler"); }
    if (sp_str_empty(t->linker))   { spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "linker"); }
    if (sp_str_empty(t->archiver)) { spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "archiver"); }
    if (sp_opt_is_null(t->driver) || sp_opt_get(t->driver) == SPN_CC_DRIVER_NONE) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "driver");
    }
    sp_da_for(t->host, h) {
      if (sp_str_empty(t->host[h].value.sha256)) {
        spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "sha256");
      }
    }
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
}

spn_err_t spn_pkg_lower(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out) {
  out->arena = sp_mem_arena_new(ctx->mem);

  lower_package(ctx, cg, out);
  lower_publish(ctx, cg, out);
  lower_targets(ctx, cg, out);
  lower_toolchains(ctx, cg, out);
  lower_profiles(cg, out);
  lower_indexes(cg, out);
  lower_deps(ctx, cg, out);
  lower_options(ctx, cg, out);
  lower_config(ctx, cg, out);

  validate_profiles(ctx, cg);
  validate_lib_linkages(ctx, out);
  validate_upstream(ctx, cg);
  validate_cxx(ctx, cg);
  validate_links(ctx, cg);
  validate_c_only_scripts(ctx, cg);
  validate_unique_targets(ctx, out);
  validate_inline_toolchains(ctx, cg);
  validate_options(ctx, cg, out);
  validate_whens(ctx, cg, out);

  return sp_da_empty(ctx->issues) ? SPN_OK : SPN_ERROR;
}

spn_err_t spn_codegen_load_pkg(spn_toml_loader_t* ctx, sp_str_t manifest, spn_pkg_info_t* out) {
  ctx->dir = sp_fs_parent_path(manifest);

  spn_cg_manifest_t cg = sp_zero;
  spn_err_t err = spn_codegen_load(ctx, manifest, &cg);
  if (err) {
    return err;
  }

  return spn_pkg_lower(ctx, &cg, out);
}
