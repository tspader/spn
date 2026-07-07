#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "forward/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "session/types.h"
#include "spn.h"
#include "unit/types.h"

#include "enum/enum.h"
#include "external/wasm/wasm.h"
#include "filter/filter.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
#include "pkg/id.h"
#include "pkg/pkg.h"
#include "profile/profile.h"
#include "session/session.h"
#include "sp/str.h"
#include "spn.embed.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

spn_err_t spn_session_init(spn_session_t* session, spn_pkg_info_t* root, spn_app_config_t config) {
  sp_str_t builtins = sp_str((const c8*)toolchains_json, toolchains_json_size);
  spn_try(spn_toolchain_catalog_init(&session->catalog, builtins, spn_triple_host(), session->mem));

  sp_str_om_for(root->toolchains, it) {
    spn_toolchain_catalog_add(&session->catalog, *sp_str_om_at(root->toolchains, it));
  }

  // Build the list of available profiles
  sp_str_ht_init(session->mem, session->profiles);
  spn_profile_populate(&session->profiles, root);

  session->pkg = root;
  session->paths.root = spn.paths.project;
  session->paths.build = sp_fs_join_path(session->mem, spn.paths.project, sp_str_lit("build"));
  session->events = spn.events;
  session->intern = spn.intern;
  sp_ht_init(session->mem, session->registry);
  sp_ht_init(session->mem, session->packages);
  sp_mutex_init(&session->mutex, SP_MUTEX_PLAIN);

  if (spn_profile_resolve(session->profiles, &config.overrides, &session->profile)) {
    sp_str_t name = spn_profile_select_name(&config.overrides);
    spn_log_error("profile {.cyan} isn't defined", SP_FMT_STR(name));
    return SPN_ERROR;
  }

  spn_toolchain_query_t query = {
    .build = session->profile.toolchain,
    .script = spn_toolchain_script_default(),
    .target = { session->profile.arch, session->profile.os, session->profile.abi },
    .host = spn_triple_host(),
  };
  spn_err_union_t select_err = spn_toolchain_select(&session->catalog, query, session->mem, &session->selection);
  if (select_err.kind) {
    spn_event_buffer_push(session->events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = select_err,
    });
    return SPN_ERROR;
  }

  session->paths.profile = sp_fs_join_path(session->mem, session->paths.build, session->profile.name);
  session->filter = config.filter;

  return SPN_OK;


  return SPN_OK;
}

spn_err_t spn_session_apply_config(spn_session_t* session, spn_config_t config) {

}

// The root manifest can pin the lib kind of any package in the build with [config.<pkg>] kind
sp_opt_spn_linkage_t spn_session_config_kind(spn_session_t* session, sp_str_t pkg_name) {
  sp_opt_spn_linkage_t requested = SP_ZERO_INITIALIZE();

  sp_da_for(session->pkg->config, it) {
    spn_pkg_config_entry_t* entry = &session->pkg->config[it];
    if (sp_str_equal(entry->key, pkg_name) && !sp_opt_is_null(entry->value.kind)) {
      sp_opt_set(requested, entry->value.kind.value);
    }
  }

  return requested;
}

typedef struct {
  sp_hash_t commit;
  sp_hash_t subtree;
  spn_semver_t version;
  spn_build_mode_t mode;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
  struct {
    sp_hash_t name;
    sp_hash_t cc;
    sp_hash_t cxx;
    sp_hash_t ld;
    sp_hash_t ar;
    sp_hash_t url;
    spn_semver_t version;
  } toolchain;
} fingerprint_input_t;

typedef struct {
  sp_hash_t hash;
  sp_str_t str;
} fingerprint_t;


fingerprint_t fingerprint_package(spn_session_t* session, spn_pkg_id_t id, spn_pkg_info_t* pkg) {
  spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, pkg->version);
  sp_assert(metadata);

  fingerprint_input_t fingerprint = SP_ZERO_INITIALIZE();
  fingerprint.commit = sp_hash_str(metadata->commit);
  fingerprint.subtree = id.hash;
  fingerprint.version = metadata->version;

  bool compiled = sp_str_om_size(pkg->libs) > 0 || sp_str_om_size(pkg->exes) > 0;
  if (compiled) {
    fingerprint.mode = session->profile.mode;
    sp_opt_spn_linkage_t config = spn_session_config_kind(session, pkg->name);
    fingerprint.linkage = config.some ? config.value : session->profile.linkage;
    fingerprint.standard = session->profile.standard;
    fingerprint.arch = session->profile.arch;
    fingerprint.os = session->profile.os;
    fingerprint.abi = session->profile.abi;

    spn_toolchain_t* toolchain = session->units.toolchain->toolchain;
    fingerprint.toolchain.cc = sp_hash_str(toolchain->compiler.program);
    fingerprint.toolchain.ld = sp_hash_str(toolchain->linker.program);
    fingerprint.toolchain.ar = sp_hash_str(toolchain->archiver.program);
    fingerprint.toolchain.cxx = sp_hash_str(toolchain->cxx.program);
    if (!sp_opt_is_null(toolchain->artifact)) {
      fingerprint.toolchain.url = sp_hash_str(sp_opt_get(toolchain->artifact).sha256);
    }
  }

  fingerprint_t result = SP_ZERO_INITIALIZE();
  result.hash = sp_hash_bytes(&fingerprint, sizeof(fingerprint), 0);
  result.str = sp_fmt(session->mem, "{:0>16x}", sp_fmt_uint(result.hash)).value;
  return result;
}

spn_pkg_unit_t* spn_session_find_pkg_by_id(spn_session_t* session, spn_pkg_id_t id) {
  sp_mutex_lock(&session->mutex);
  spn_pkg_unit_t* pkg = sp_om_get(session->units.packages, id);
  sp_mutex_unlock(&session->mutex);

  return pkg;
}

spn_pkg_unit_t* spn_session_find_dep(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t qualified, spn_dep_kind_t kind) {
  sp_intern_id_t name = sp_intern_get_or_insert(session->intern, qualified);

  sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, pkg);
  sp_da_for(deps, it) {
    if (deps[it].kind != kind) {
      continue;
    }
    if (deps[it].unit && deps[it].unit->id.qualified == name) {
      return deps[it].unit;
    }
  }
  return SP_NULLPTR;
}

spn_target_unit_t* spn_session_find_target_in_pkg(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t name) {
  spn_target_unit_id_t id = {
    .pkg = pkg->id,
    .target = sp_intern_get_or_insert(session->intern, name)
  };
  if (!sp_om_has(session->units.targets, id)) return SP_NULLPTR;
  return sp_om_get(session->units.targets, id);
}

spn_pkg_unit_t* spn_session_find_root(spn_session_t* s) {
  sp_ht_for_kv(s->resolve, it) {
    if (it.val->source == SPN_PKG_SOURCE_ROOT && sp_str_equal(it.val->qualified, s->pkg->qualified)) {
      return spn_session_find_pkg_by_id(s, it.val->id);
    }
  }
  return SP_NULLPTR;
}

sp_da(spn_pkg_dep_t) spn_session_pkg_deps(spn_session_t* session, spn_pkg_unit_t* pkg) {
  if (!sp_om_has(session->units.graph, pkg->id)) return SP_NULLPTR;
  return *sp_om_get(session->units.graph, pkg->id);
}

spn_target_unit_t* spn_session_add_target(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  spn_target_unit_id_t id = {
    .pkg = pkg->id,
    .target = sp_intern_get_or_insert(session->intern, info->name),
  };

  sp_om_insert(session->units.targets, id, SP_ZERO_STRUCT(spn_target_unit_t));
  spn_target_unit_t* target = sp_om_back(session->units.targets);
  target->id = id;
  target->session = session;
  target->pkg = pkg;
  target->info = info;
  sp_da_init(session->mem, target->objects);
  sp_da_init(session->mem, target->deps.target);
  sp_da_init(session->mem, target->deps.package);
  sp_da_init(session->mem, target->nodes.source);

  sp_da_push(pkg->targets, target);
  switch (info->kind) {
    case SPN_TARGET_LIB: sp_da_push(pkg->libs, target); break;
    case SPN_TARGET_EXE: sp_da_push(pkg->exes, target); break;
    case SPN_TARGET_SCRIPT: sp_da_push(pkg->scripts, target); break;
    case SPN_TARGET_TEST: sp_da_push(pkg->tests, target); break;
  }

  target->paths.source = pkg->paths.source;
  target->paths.work = pkg->paths.work;
  target->paths.store = pkg->paths.store;
  target->paths.include = sp_fs_join_path(session->mem, target->paths.store, SP_LIT("include"));
  target->paths.bin = sp_fs_join_path(session->mem, target->paths.store, SP_LIT("bin"));
  target->paths.lib = sp_fs_join_path(session->mem, target->paths.store, SP_LIT("lib"));
  target->paths.vendor = sp_fs_join_path(session->mem, target->paths.store, SP_LIT("vendor"));
  target->paths.generated = sp_fs_join_path(session->mem, target->paths.work, SP_LIT("spn"));
  target->paths.object = sp_fs_join_path(session->mem, target->paths.generated, sp_str_lit("object"));
  target->paths.logs.build = sp_fs_join_path(session->mem, target->paths.work, sp_fmt(session->mem, "{}.build.log", SP_FMT_STR(info->name)).value);
  target->paths.logs.test = sp_fs_join_path(session->mem, target->paths.work, sp_fmt(session->mem, "{}.test.log", SP_FMT_STR(info->name)).value);
  target->paths.logs.jsonl = sp_fs_join_path(session->mem, target->paths.work, sp_fmt(session->mem, "{}.build.jsonl", SP_FMT_STR(info->name)).value);

  sp_fs_create_dir(target->paths.work);
  sp_fs_create_dir(target->paths.generated);
  sp_fs_create_dir(target->paths.object);
  sp_fs_create_dir(target->paths.store);
  sp_fs_create_dir(target->paths.bin);
  sp_fs_create_dir(target->paths.include);
  sp_fs_create_dir(target->paths.lib);
  sp_fs_create_dir(target->paths.vendor);
  spn_lazy_log_init(&target->logs.build, target->paths.logs.build);
  spn_lazy_log_init(&target->logs.jsonl, target->paths.logs.jsonl);
  return target;
}

spn_pkg_unit_t* spn_session_add_pkg(spn_session_t* session, spn_pkg_id_t id, spn_loaded_pkg_t* loaded) {
  sp_om_insert(session->units.packages, id, sp_zero_struct(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  unit->id = id;
  unit->info = loaded->info;
  unit->session = session;
  sp_da_init(session->mem, unit->objects);
  sp_da_init(session->mem, unit->libs);
  sp_da_init(session->mem, unit->exes);
  sp_da_init(session->mem, unit->scripts);
  sp_da_init(session->mem, unit->tests);
  sp_da_init(session->mem, unit->targets);
  sp_da_init(spn.mem, unit->nodes.user);
  sp_da_init(session->mem, unit->nodes.build.user);
  sp_str_ht_init(session->mem, unit->nodes.files);
  unit->paths.manifest = loaded->paths.manifest;
  unit->paths.script = loaded->paths.script;
  unit->paths.source = loaded->roots.source;

  switch (loaded->source) {
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE: {
      sp_str_t work = sp_fs_join_path(session->mem, session->paths.profile, sp_str_lit("work"));
      unit->paths.work = sp_fs_join_path(session->mem, work, loaded->info->name);
      unit->paths.store = sp_fs_join_path(session->mem, session->paths.profile, sp_str_lit("store"));
      break;
    }
    case SPN_PKG_SOURCE_INDEX: {
      fingerprint_t fingerprint = fingerprint_package(session, id, loaded->info);
      unit->paths.work = sp_fs_join_path(session->mem, sp_fs_join_path(session->mem, spn.paths.build, loaded->info->qualified), fingerprint.str);
      unit->paths.store = sp_fs_join_path(session->mem, sp_fs_join_path(session->mem, spn.paths.store, loaded->info->qualified), fingerprint.str);
      break;
    }
  }

  unit->paths.include = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("vendor"));

  unit->paths.generated = sp_fs_join_path(session->mem, unit->paths.work, SP_LIT("spn"));
  spn_wasm_script_init(&unit->wasm.configure, loaded->paths.configure, sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("configure.wasm")));
  spn_wasm_script_init(&unit->wasm.build, loaded->paths.build, sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("build.wasm")));

  unit->logs.build = sp_fmt(session->mem, "{}.build.log", SP_FMT_STR(unit->info->name)).value;
  unit->logs.test = sp_fmt(session->mem, "{}.test.log", SP_FMT_STR(unit->info->name)).value;
  unit->logs.jsonl = sp_fmt(session->mem, "{}.jsonl", SP_FMT_STR(unit->info->name)).value;

  unit->paths.logs.build = sp_fs_join_path(session->mem, unit->paths.work, unit->logs.build);
  unit->paths.logs.test = sp_fs_join_path(session->mem, unit->paths.work, unit->logs.test);
  unit->paths.logs.jsonl = sp_fs_join_path(session->mem, unit->paths.work, unit->logs.jsonl);

  sp_fs_create_dir(unit->paths.work);
  sp_fs_create_dir(unit->paths.generated);
  sp_fs_create_dir(unit->paths.store);
  sp_fs_create_dir(unit->paths.bin);
  sp_fs_create_dir(unit->paths.include);
  sp_fs_create_dir(unit->paths.lib);
  sp_fs_create_dir(unit->paths.vendor);
  spn_lazy_log_init(&unit->logs.io.build, unit->paths.logs.build);
  spn_lazy_log_init(&unit->logs.io.jsonl, unit->paths.logs.jsonl);

  unit->paths.stamp.dir = sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("stamp"));
  unit->paths.stamp.main = sp_fs_join_path(session->mem, unit->paths.stamp.dir, SP_LIT("main.stamp"));
  unit->paths.stamp.exit = sp_fs_join_path(session->mem, unit->paths.stamp.dir, SP_LIT("user.stamp"));
  unit->paths.stamp.configure = sp_fs_join_path(session->mem, unit->paths.stamp.dir, SP_LIT("configure.stamp"));
  unit->paths.stamp.build = sp_fs_join_path(session->mem, unit->paths.stamp.dir, SP_LIT("build.stamp"));
  unit->paths.stamp.package = sp_fs_join_path(session->mem, unit->paths.stamp.dir, SP_LIT("package.stamp"));

  sp_fs_create_dir(unit->paths.stamp.dir);

  return unit;
}
