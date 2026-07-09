#include "sp.h"
#include "sp/macro.h"
#include "forward/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "session/types.h"
#include "spn.h"
#include "unit/types.h"

#include "enum/enum.h"
#include "event/event.h"
#include "event/types.h"
#include "external/wasm/wasm.h"
#include "filter/filter.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
#include "pkg/id.h"
#include "pkg/pkg.h"
#include "profile/profile.h"
#include "session/session.h"
#include "when/when.h"
#include "sp/str.h"
#include "spn.embed.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

spn_err_union_t spn_session_init(spn_session_t* s, sp_mem_t mem, spn_pkg_info_t* root, spn_app_config_t config) {
  s->mem = mem;
  sp_str_t builtins = sp_str((const c8*)toolchains_json, toolchains_json_size);
  try_as_union(spn_toolchain_catalog_init(&s->catalog, builtins, spn_triple_host(), s->mem));

  sp_str_om_for(root->toolchains, it) {
    spn_toolchain_catalog_add(&s->catalog, *sp_str_om_at(root->toolchains, it));
  }

  // Build the list of available profiles
  sp_str_ht_init(s->mem, s->profiles);
  spn_profile_populate(&s->profiles, root);

  s->pkg = root;
  s->paths.build = sp_fs_join_path(s->mem, s->paths.root, sp_str_lit("build"));
  sp_ht_init(s->mem, s->registry);
  sp_ht_init(s->mem, s->packages);
  sp_mutex_init(&s->mutex, SP_MUTEX_PLAIN);

  try_union(spn_profile_resolve(s->profiles, &config.overrides, &s->profile));

  spn_toolchain_query_t query = {
    .build = s->profile.toolchain,
    .script = spn_toolchain_script_default(),
    .target = { s->profile.arch, s->profile.os, s->profile.abi },
    .host = spn_triple_host(),
  };
  try_union(spn_toolchain_select(&s->catalog, query, s->mem, &s->selection));

  s->paths.profile = sp_fs_join_path(s->mem, s->paths.build, s->profile.name);
  s->filter = config.filter;

  return spn_result(SPN_OK);
}

// Options resolve once every manifest is loaded and every edge is known:
// requests are read off consumers' dep entries, merged per package with the
// root's config, and the result drives one apply per loaded info. The gated
// dep recheck compares each predicate against what resolution actually did,
// since resolve gated local packages' edges before requests existed and
// index packages' edges not at all.
static sp_str_t node_short_name(spn_resolved_pkg_t* node) {
  return spn_pkg_name_from_qualified(node->qualified).name;
}

static bool node_has_edge(spn_resolved_pkg_t* node, sp_intern_id_t qualified, spn_dep_kind_t kind) {
  sp_da_for(node->edges, it) {
    if (node->edges[it].id.qualified == qualified && node->edges[it].kind == kind) {
      return true;
    }
  }
  return false;
}

spn_err_t spn_session_apply_options(spn_session_t* session) {
  sp_mem_t mem = session->mem;
  sp_str_ht_init(mem, session->options);

  sp_str_ht(sp_da(spn_option_request_t)) requests = SP_NULLPTR;
  sp_str_ht_init(mem, requests);

  sp_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* node = it.val;
    spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
    sp_assert(loaded);

    sp_da_for(loaded->info->deps, dt) {
      spn_requested_pkg_t* dep = &loaded->info->deps[dt];
      if (sp_da_empty(dep->options.clauses)) {
        continue;
      }
      if (!node_has_edge(node, sp_intern_get_or_insert(session->intern, dep->qualified), dep->kind)) {
        continue;
      }

      if (!sp_str_ht_get(requests, dep->qualified)) {
        sp_str_ht_insert(requests, dep->qualified, sp_da_new(mem, spn_option_request_t));
      }
      sp_da_push(*sp_str_ht_get(requests, dep->qualified), ((spn_option_request_t) {
        .consumer = node_short_name(node),
        .options = &dep->options,
      }));
    }
  }

  sp_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* node = it.val;
    if (sp_str_ht_get(session->options, node->qualified)) {
      continue;
    }

    spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
    sp_da(spn_option_request_t)* asked = sp_str_ht_get(requests, node->qualified);

    spn_resolved_options_t resolved = sp_zero;
    spn_try(spn_pkg_options_merge(
      mem,
      loaded->info,
      &session->profile,
      session->pkg->config,
      node->source == SPN_PKG_SOURCE_ROOT,
      asked ? *asked : SP_NULLPTR,
      session->events,
      &resolved));

    sp_str_ht_insert(session->options, node->qualified, resolved);
  }

  sp_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* node = it.val;
    spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
    spn_resolved_options_t* resolved = sp_str_ht_get(session->options, loaded->info->qualified);
    sp_assert(resolved);

    spn_when_env_t env;
    spn_when_env_from_profile(mem, &session->profile, &env);
    spn_when_env_add_options(&env, resolved);

    sp_da_for(loaded->info->deps, dt) {
      spn_requested_pkg_t* dep = &loaded->info->deps[dt];
      if (dep->kind != SPN_DEP_KIND_PACKAGE || sp_da_empty(dep->when.clauses)) {
        continue;
      }
      bool expected = spn_when_eval(&dep->when, &env);
      bool actual = node_has_edge(node, sp_intern_get_or_insert(session->intern, dep->qualified), dep->kind);
      if (expected != actual) {
        spn_event_buffer_push(session->events, (spn_build_event_t) {
          .kind = SPN_EVENT_ERR_OPTION,
          .option = {
            .err = SPN_OPTION_ERR_LATE_GATE,
            .pkg = loaded->info->name,
            .a = spn_pkg_name_from_qualified(dep->qualified).name,
          },
        });
        return SPN_ERROR;
      }
    }

    spn_pkg_apply_options(loaded->info, &env);
  }

  return SPN_OK;
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
  sp_hash_t options;
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

// The resolved non-default option set: distinct option sets get distinct
// store paths, while default flips ride the manifest commit hash instead
static sp_hash_t hash_options(spn_session_t* session, spn_pkg_info_t* pkg) {
  spn_resolved_options_t* options = sp_str_ht_get(session->options, pkg->qualified);
  if (!options) {
    return 0;
  }

  sp_hash_t hash = 0;
  sp_da_for(*options, it) {
    spn_resolved_option_t* option = &(*options)[it];
    if (option->is_default) {
      continue;
    }
    sp_hash_t parts [] = {
      hash,
      sp_hash_str(option->name),
      (sp_hash_t)option->value.kind,
      option->value.kind == SPN_OPTION_VALUE_STR ? sp_hash_str(option->value.str) : (sp_hash_t)option->value.b,
    };
    hash = sp_hash_combine(parts, sp_carr_len(parts));
  }
  return hash;
}

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
  fingerprint.options = hash_options(session, pkg);
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
  unit->configure = loaded->configure;
  unit->build = loaded->build;
  unit->session = session;
  sp_da_init(session->mem, unit->objects);
  sp_da_init(session->mem, unit->libs);
  sp_da_init(session->mem, unit->exes);
  sp_da_init(session->mem, unit->scripts);
  sp_da_init(session->mem, unit->tests);
  sp_da_init(session->mem, unit->targets);
  sp_da_init(session->mem, unit->nodes.user);
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
      unit->paths.work = sp_fs_join_path(session->mem, sp_fs_join_path(session->mem, session->paths.system.caches.build.dir, loaded->info->qualified), fingerprint.str);
      unit->paths.store = sp_fs_join_path(session->mem, sp_fs_join_path(session->mem, session->paths.system.caches.store.dir, loaded->info->qualified), fingerprint.str);
      break;
    }
  }

  unit->paths.include = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("include"));
  unit->paths.bin = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = sp_fs_join_path(session->mem, unit->paths.store, SP_LIT("vendor"));

  unit->paths.generated = sp_fs_join_path(session->mem, unit->paths.work, SP_LIT("spn"));
  spn_wasm_script_init(&unit->wasm.configure, !sp_da_empty(unit->configure.source), sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("configure.wasm")));
  spn_wasm_script_init(&unit->wasm.build, !sp_da_empty(unit->build.source), sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("build.wasm")));

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
