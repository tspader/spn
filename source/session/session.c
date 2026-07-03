#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "forward/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "session/types.h"
#include "unit/types.h"

#include "enum/enum.h"
#include "filter/filter.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
#include "pkg/pkg.h"
#include "session/session.h"
#include "sp/str.h"

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


fingerprint_t fingerprint_package(spn_session_t* session, spn_pkg_info_t* pkg) {
  spn_pkg_metadata_t* metadata = sp_ht_getp(pkg->metadata, pkg->version);
  sp_assert(metadata);

  fingerprint_input_t fingerprint = SP_ZERO_INITIALIZE();
  fingerprint.commit = sp_hash_str(metadata->commit);
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
    if (!sp_opt_is_null(toolchain->artifact)) {
      fingerprint.toolchain.url = sp_hash_str(sp_opt_get(toolchain->artifact).sha256);
    }
  }

  fingerprint_t id = SP_ZERO_INITIALIZE();
  id.hash = sp_hash_bytes(&fingerprint, sizeof(fingerprint), 0);
  id.str = sp_fmt(session->mem, "{:0>16x}", sp_fmt_uint(id.hash)).value;
  return id;
}

spn_pkg_unit_t* spn_session_find_pkg_by_qualified(spn_session_t* session, sp_str_t qualified) {
  spn_pkg_unit_id_t id = { sp_intern_get_or_insert(session->intern, qualified) };
  sp_mutex_lock(&session->mutex);
  spn_pkg_unit_t* pkg = sp_om_get(session->units.packages, id);
  sp_mutex_unlock(&session->mutex);

  return pkg;
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
  return spn_session_find_pkg_by_qualified(s, s->pkg->qualified);
}

sp_da(spn_pkg_unit_t*) spn_session_pkg_deps(spn_session_t* session, spn_pkg_unit_t* pkg) {
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

spn_pkg_unit_t* spn_session_add_pkg(spn_session_t* session, spn_loaded_pkg_t* loaded) {
  spn_pkg_unit_id_t id = { sp_intern_get_or_insert(session->intern, loaded->info->qualified) };

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
  unit->paths.configure = loaded->paths.configure;
  unit->paths.build = loaded->paths.build;
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
      fingerprint_t id = fingerprint_package(session, loaded->info);
      unit->paths.work = sp_fs_join_path(session->mem, sp_fs_join_path(session->mem, spn.paths.build, loaded->info->qualified), id.str);
      unit->paths.store = sp_fs_join_path(session->mem, sp_fs_join_path(session->mem, spn.paths.store, loaded->info->qualified), id.str);
      break;
    }
  }

  unit->paths.include = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("vendor"));

  unit->paths.generated = sp_fs_join_path(session->mem, unit->paths.work, SP_LIT("spn"));
  unit->paths.wasm.configure = sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("configure.wasm"));
  unit->paths.wasm.build = sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("build.wasm"));

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
