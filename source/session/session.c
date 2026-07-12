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
  sp_ht_init(s->mem, s->fingerprints);
  sp_mutex_init(&s->mutex, SP_MUTEX_PLAIN);

  try_union(spn_profile_resolve(s->profiles, &config.overrides, &s->profile));

  spn_toolchain_query_t query = {
    .build = s->profile.toolchain,
    .script = spn_toolchain_script_default(),
    .target = { s->profile.arch, s->profile.os, s->profile.abi },
    .host = spn_triple_host(),
  };
  try_union(spn_toolchain_select(&s->catalog, query, s->mem, &s->selection));

  sp_str_t profile = s->paths.build;
  if (s->profile.targeted) {
    spn_triple_t target = { s->profile.arch, s->profile.os, s->profile.abi };
    profile = sp_fs_join_path(s->mem, profile, spn_triple_to_str(s->mem, target));
  }
  s->paths.profile = sp_fs_join_path(s->mem, profile, s->profile.name);

  s->plan.request = config.compile;

  return spn_result(SPN_OK);
}

#define SPN_GATE_MAX_RESOLVES 4

static spn_resolved_dep_t* node_find_edge(spn_resolved_pkg_t* node, sp_intern_id_t qualified, spn_dep_kind_t kind) {
  sp_da_for(node->edges, it) {
    if (node->edges[it].id.qualified == qualified && node->edges[it].kind == kind) {
      return &node->edges[it];
    }
  }
  return SP_NULLPTR;
}

static void node_prune_edge(spn_resolved_pkg_t* node, spn_resolved_dep_t* edge) {
  u64 index = (u64)(edge - node->edges);
  for (u64 it = index + 1; it < sp_da_size(node->edges); it++) {
    node->edges[it - 1] = node->edges[it];
  }
  sp_da_pop(node->edges);
}

static void sweep_unreachable(spn_session_t* session) {
  sp_mem_t mem = session->mem;

  sp_ht(spn_pkg_id_t, u8) visited = SP_NULLPTR;
  sp_ht_init(mem, visited);
  sp_da(spn_pkg_id_t) frontier = sp_da_new(mem, spn_pkg_id_t);

  sp_ht_for_kv(session->resolve, it) {
    if (it.val->source == SPN_PKG_SOURCE_ROOT) {
      sp_ht_insert(visited, it.val->id, (u8)true);
      sp_da_push(frontier, it.val->id);
    }
  }

  while (!sp_da_empty(frontier)) {
    spn_pkg_id_t id = *sp_da_back(frontier);
    sp_da_pop(frontier);

    spn_resolved_pkg_t* node = sp_ht_getp(session->resolve, id);
    if (!node) {
      continue;
    }
    sp_da_for(node->edges, et) {
      if (!sp_ht_getp(visited, node->edges[et].id)) {
        sp_ht_insert(visited, node->edges[et].id, (u8)true);
        sp_da_push(frontier, node->edges[et].id);
      }
    }
  }

  spn_resolve_t kept = SP_NULLPTR;
  sp_ht_init(mem, kept);
  sp_ht_for_kv(session->resolve, it) {
    if (sp_ht_getp(visited, it.val->id)) {
      sp_ht_insert(kept, *it.key, *it.val);
    }
  }
  session->resolve = kept;
}

static spn_err_t validate_config_keys(spn_session_t* session) {
  sp_da_for(session->pkg->config, ct) {
    spn_pkg_config_entry_t* entry = &session->pkg->config[ct];

    bool known = false;
    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
      sp_assert(loaded);
      if (sp_str_equal(node->name, entry->key)) {
        known = true;
        break;
      }
      sp_da_for(loaded->info->deps, dt) {
        if (sp_str_equal(spn_pkg_name_from_qualified(loaded->info->deps[dt].qualified).name, entry->key)) {
          known = true;
          break;
        }
      }
      if (known) {
        break;
      }
    }

    if (!known) {
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_OPTION,
        .option = {
          .err = SPN_OPTION_ERR_UNKNOWN_PKG,
          .pkg = entry->key,
        },
      });
      return SPN_ERROR;
    }
  }
  return SPN_OK;
}

spn_err_t spn_session_apply_options(spn_session_t* session) {
  sp_mem_t mem = session->mem;

  sp_str_t missing_pkg = sp_zero;
  sp_str_t missing_dep = sp_zero;

  // Pruning is monotone (every pass removes at least one edge), so it runs
  // to fixpoint; only the resolve direction is capped
  for (;;) {
    sp_ht_init(mem, session->options);
    sp_ht_init(mem, session->gates.seeds);

    sp_ht(spn_pkg_id_t, spn_option_requests_t) requests = SP_NULLPTR;
    sp_ht_init(mem, requests);

    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
      sp_assert(loaded);

      sp_da_for(loaded->info->deps, dt) {
        spn_requested_dep_t* dep = &loaded->info->deps[dt];
        if (sp_da_empty(dep->options.clauses)) {
          continue;
        }
        spn_resolved_dep_t* edge = node_find_edge(node, sp_intern_get_or_insert(session->intern, dep->qualified), dep->kind);
        if (!edge) {
          continue;
        }

        spn_option_request_t request = {
          .consumer = node->name,
          .options = &dep->options,
        };

        if (!sp_ht_getp(requests, edge->id)) {
          sp_ht_insert(requests, edge->id, sp_da_new(mem, spn_option_request_t));
        }
        sp_da_push(*sp_ht_getp(requests, edge->id), request);

        // Seeds feed a possible next resolve pass, which looks packages up
        // by the qualified name their manifest declares
        spn_resolved_pkg_t* target = sp_ht_getp(session->resolve, edge->id);
        sp_intern_id_t seed_key = target ? target->id.qualified : sp_intern_get_or_insert(session->intern, dep->qualified);
        if (!sp_ht_getp(session->gates.seeds, seed_key)) {
          sp_ht_insert(session->gates.seeds, seed_key, sp_da_new(mem, spn_option_request_t));
        }
        sp_da_push(*sp_ht_getp(session->gates.seeds, seed_key), request);
      }
    }

    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_option_requests_t* asked = sp_ht_getp(requests, node->id);

      spn_resolved_options_t resolved = sp_zero;
      spn_try(spn_pkg_options_merge(
        mem,
        node,
        &session->profile,
        session->pkg->config,
        asked ? *asked : SP_NULLPTR,
        session->events,
        &resolved));

      sp_ht_insert(session->options, node->id, resolved);
    }

    bool pruned = false;
    bool missing = false;
    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
      spn_resolved_options_t* resolved = sp_ht_getp(session->options, node->id);
      sp_assert(resolved);

      spn_when_env_t env;
      spn_when_env_from_profile(mem, &session->profile, &env);
      spn_when_env_add_options(&env, resolved);

      sp_da_for(loaded->info->deps, dt) {
        spn_requested_dep_t* dep = &loaded->info->deps[dt];
        if (sp_da_empty(dep->when.clauses)) {
          continue;
        }
        spn_resolved_dep_t* edge = node_find_edge(node, sp_intern_get_or_insert(session->intern, dep->qualified), dep->kind);
        bool expected = spn_when_eval(&dep->when, &env);
        if (expected && !edge) {
          missing = true;
          if (sp_str_empty(missing_pkg)) {
            missing_pkg = node->name;
            missing_dep = spn_pkg_name_from_qualified(dep->qualified).name;
          }
        }
        if (!expected && edge) {
          node_prune_edge(node, edge);
          pruned = true;
        }
      }
    }

    if (pruned) {
      sweep_unreachable(session);
      continue;
    }
    if (missing) {
      if (session->gates.resolves < SPN_GATE_MAX_RESOLVES) {
        session->gates.resolves++;
        session->gates.reresolve = true;
        return SPN_OK;
      }
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_OPTION,
        .option = {
          .err = SPN_OPTION_ERR_LATE_GATE,
          .pkg = missing_pkg,
          .a = { .kind = SPN_OPTION_SETTER_CONSUMER, .name = missing_dep },
        },
      });
      return SPN_ERROR;
    }
    break;
  }

  spn_try(validate_config_keys(session));

  return SPN_OK;
}

// The root manifest can pin the lib kind of any package in the build with [config.<pkg>] kind
sp_opt_spn_linkage_t spn_session_config_kind(spn_session_t* session, sp_str_t pkg_name) {
  sp_opt_spn_linkage_t requested = SP_ZERO_INITIALIZE();

  spn_pkg_config_t* config = spn_pkg_config_find(session->pkg->config, pkg_name);
  if (config && !sp_opt_is_null(config->kind)) {
    sp_opt_set(requested, config->kind.value);
  }

  return requested;
}

typedef struct {
  sp_hash_t qualified;
  sp_hash_t commit;
  sp_hash_t options;
  sp_hash_t deps;
  spn_semver_t version;
  spn_build_kind_t kind;
  spn_build_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
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
    sp_hash_t identity;
  } toolchain;
} fingerprint_input_t;

// The resolved non-default option set: distinct option sets get distinct
// store paths, while default flips ride the manifest commit hash instead
static sp_hash_t hash_options(spn_session_t* session, spn_pkg_id_t id) {
  spn_resolved_options_t* options = sp_ht_getp(session->options, id);
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
  u32 kind;
  u32 private;
} fingerprint_edge_t;

static s32 sort_fingerprint_edges(const void* a, const void* b) {
  const fingerprint_edge_t* lhs = (const fingerprint_edge_t*)a;
  const fingerprint_edge_t* rhs = (const fingerprint_edge_t*)b;
  if (lhs->hash != rhs->hash) return lhs->hash < rhs->hash ? -1 : 1;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->private != rhs->private) return lhs->private < rhs->private ? -1 : 1;
  return 0;
}

static sp_hash_t hash_package(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t id) {
  spn_pkg_unit_id_t uid = { .pkg = id, .ctx = build->id };
  sp_hash_t* memo = sp_ht_getp(session->fingerprints, uid);
  if (memo) {
    return *memo;
  }

  spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, id);
  spn_pkg_info_t* pkg = loaded->info;

  fingerprint_input_t fingerprint = SP_ZERO_INITIALIZE();
  fingerprint.qualified = sp_hash_str(pkg->qualified);
  fingerprint.options = hash_options(session, id);
  fingerprint.version = pkg->version;
  fingerprint.commit = sp_hash_str(pkg->upstream.commit);
  fingerprint.kind = build->kind;

  spn_resolved_pkg_t* resolved = sp_ht_getp(session->resolve, id);
  if (resolved) {
    if (!sp_da_empty(resolved->edges)) {
      sp_da(fingerprint_edge_t) edges = sp_da_new(session->mem, fingerprint_edge_t);
      sp_da_for(resolved->edges, it) {
        sp_da_push(edges, ((fingerprint_edge_t) {
          .hash = hash_package(session, build, resolved->edges[it].id),
          .kind = (u32)resolved->edges[it].kind,
          .private = (u32)resolved->edges[it].private,
        }));
      }
      sp_da_sort(edges, sort_fingerprint_edges);
      fingerprint.deps = sp_hash_bytes(edges, sp_da_size(edges) * sizeof(fingerprint_edge_t), 0);
    }
  }

  spn_toolchain_t* toolchain = build->toolchain->toolchain;
  sp_opt_spn_linkage_t config = spn_session_config_kind(session, pkg->name);

  fingerprint.mode = build->profile.mode;
  fingerprint.opt = build->profile.opt;
  fingerprint.sanitizers = build->profile.sanitizers;
  fingerprint.linkage = config.some ? config.value : build->profile.linkage;
  fingerprint.standard = build->profile.standard;
  fingerprint.arch = build->profile.arch;
  fingerprint.os = build->profile.os;
  fingerprint.abi = build->profile.abi;
  fingerprint.toolchain.name = sp_hash_str(toolchain->name);
  fingerprint.toolchain.cc = sp_hash_str(toolchain->compiler.program);
  fingerprint.toolchain.ld = sp_hash_str(toolchain->linker.program);
  fingerprint.toolchain.ar = sp_hash_str(toolchain->archiver.program);
  fingerprint.toolchain.cxx = sp_hash_str(toolchain->cxx.program);
  fingerprint.toolchain.identity = build->toolchain->identity;
  if (!sp_opt_is_null(toolchain->artifact)) {
    fingerprint.toolchain.url = sp_hash_str(sp_opt_get(toolchain->artifact).sha256);
  }

  sp_hash_t hash = sp_hash_bytes(&fingerprint, sizeof(fingerprint), 0);
  sp_ht_insert(session->fingerprints, uid, hash);
  return hash;
}

typedef struct {
  sp_hash_t hash;
  sp_str_t str;
} fingerprint_t;

fingerprint_t fingerprint_package(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t id) {
  fingerprint_t result = SP_ZERO_INITIALIZE();
  result.hash = hash_package(session, build, id);
  result.str = sp_fmt(session->mem, "{:0>16x}", sp_fmt_uint(result.hash)).value;
  return result;
}

spn_pkg_unit_t* spn_session_find_pkg_unit_by_id(spn_session_t* session, spn_pkg_unit_id_t id) {
  sp_mutex_lock(&session->mutex);
  spn_pkg_unit_t* pkg = sp_om_has(session->units.packages, id) ? sp_om_get(session->units.packages, id) : SP_NULLPTR;
  sp_mutex_unlock(&session->mutex);

  return pkg;
}

spn_pkg_unit_t* spn_session_find_pkg_unit(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t pkg_id) {
  return spn_session_find_pkg_unit_by_id(session, (spn_pkg_unit_id_t) {
    .pkg = pkg_id,
    .ctx = build->id,
  });
}

spn_pkg_unit_t* spn_session_find_dep(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t qualified, spn_dep_kind_t kind) {
  sp_intern_id_t name = sp_intern_get_or_insert(session->intern, qualified);

  sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, pkg);
  sp_da_for(deps, it) {
    if (deps[it].kind != kind) {
      continue;
    }
    if (deps[it].unit && deps[it].unit->id.pkg.qualified == name) {
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

spn_target_unit_t* spn_session_get_target_unit(spn_session_t* session, spn_target_unit_id_t id) {
  sp_assert(sp_om_has(session->units.targets, id));
  return sp_om_get(session->units.targets, id);
}

spn_pkg_id_t spn_session_root_pkg(spn_session_t* session) {
  sp_ht_for_kv(session->resolve, it) {
    if (it.val->source == SPN_PKG_SOURCE_ROOT) {
      return it.val->id;
    }
  }
  return SP_ZERO_STRUCT(spn_pkg_id_t);
}

spn_build_plan_t* spn_session_find_plan(spn_session_t* session, spn_build_kind_t kind) {
  sp_da_for(session->plan.builds, it) {
    if (session->plan.builds[it].build->kind == kind) {
      return &session->plan.builds[it];
    }
  }
  return SP_NULLPTR;
}

spn_build_plan_t* spn_session_plan_for_build(spn_session_t* session, spn_build_unit_t* build) {
  sp_da_for(session->plan.builds, it) {
    if (session->plan.builds[it].build == build) {
      return &session->plan.builds[it];
    }
  }
  return SP_NULLPTR;
}

sp_da(spn_pkg_dep_t) spn_session_pkg_deps(spn_session_t* session, spn_pkg_unit_t* pkg) {
  return pkg->deps;
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

  target->paths.recipe = pkg->paths.recipe;
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

static sp_da(sp_str_t) clone_str_list(sp_mem_t mem, sp_da(sp_str_t) source) {
  sp_da(sp_str_t) result = sp_da_new(mem, sp_str_t);
  sp_da_for(source, it) {
    sp_da_push(result, source[it]);
  }
  return result;
}

static sp_da(spn_embed_t) clone_embed_list(sp_mem_t mem, sp_da(spn_embed_t) source) {
  sp_da(spn_embed_t) result = sp_da_new(mem, spn_embed_t);
  sp_da_for(source, it) {
    sp_da_push(result, source[it]);
  }
  return result;
}

static spn_target_info_t clone_target_info(sp_mem_t mem, spn_target_info_t* source) {
  spn_target_info_t target = *source;
  target.source = clone_str_list(mem, source->source);
  target.headers = clone_str_list(mem, source->headers);
  target.include = clone_str_list(mem, source->include);
  target.define = clone_str_list(mem, source->define);
  target.flags = clone_str_list(mem, source->flags);
  target.system_deps = clone_str_list(mem, source->system_deps);
  target.deps = clone_str_list(mem, source->deps);
  target.embed = clone_embed_list(mem, source->embed);
  return target;
}

static void clone_target_map(spn_target_map_t* result, spn_target_map_t source, sp_mem_t mem) {
  sp_str_om_init(*result);
  sp_str_om_for(source, it) {
    spn_target_info_t target = clone_target_info(mem, sp_str_om_at(source, it));
    sp_str_om_insert(*result, target.name, target);
  }
}

static spn_pkg_info_t* clone_pkg_info(spn_session_t* session, spn_pkg_id_t id, spn_build_unit_t* build, spn_pkg_info_t* source) {
  spn_pkg_info_t* info = sp_alloc_type(session->mem, spn_pkg_info_t);
  *info = *source;
  info->arena = sp_mem_arena_new(session->mem);
  info->applied = false;
  sp_mem_t mem = sp_mem_arena_as_allocator(info->arena);

  clone_target_map(&info->libs, source->libs, mem);
  clone_target_map(&info->exes, source->exes, mem);
  clone_target_map(&info->scripts, source->scripts, mem);
  clone_target_map(&info->tests, source->tests, mem);
  info->include = clone_str_list(mem, source->include);
  info->define = clone_str_list(mem, source->define);
  info->public_define = clone_str_list(mem, source->public_define);
  info->system_deps = clone_str_list(mem, source->system_deps);

  spn_when_env_t env;
  spn_when_env_from_profile(mem, &build->profile, &env);
  spn_resolved_options_t* resolved = sp_ht_getp(session->options, id);
  if (resolved) {
    spn_when_env_add_options(&env, resolved);
  }
  spn_pkg_apply_options(info, &env);
  return info;
}

spn_pkg_unit_t* spn_session_add_pkg_unit(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t pkg_id, spn_loaded_pkg_t* loaded) {
  spn_pkg_unit_id_t id = { .pkg = pkg_id, .ctx = build->id };
  sp_om_insert(session->units.packages, id, sp_zero_struct(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  unit->id = id;
  unit->build = build;
  unit->info = clone_pkg_info(session, pkg_id, build, loaded->info);
  unit->source = loaded->source;
  unit->script.configure = loaded->configure;
  unit->script.build = loaded->build;
  unit->session = session;
  sp_da_init(session->mem, unit->objects);
  sp_da_init(session->mem, unit->deps);
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
  unit->paths.recipe = loaded->roots.recipe;
  unit->paths.source = loaded->roots.source;

  switch (loaded->source) {
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE: {
      sp_str_t work = sp_fs_join_path(session->mem, build->paths.profile, sp_str_lit("work"));
      unit->paths.work = sp_fs_join_path(session->mem, work, loaded->info->name);
      unit->paths.store = sp_fs_join_path(session->mem, build->paths.profile, sp_str_lit("store"));
      break;
    }
    case SPN_PKG_SOURCE_INDEX: {
      fingerprint_t fingerprint = fingerprint_package(session, build, pkg_id);
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
  spn_wasm_script_init(&unit->wasm.configure, !sp_da_empty(unit->script.configure.source), sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("configure.wasm")));
  spn_wasm_script_init(&unit->wasm.build, !sp_da_empty(unit->script.build.source), sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("build.wasm")));

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
  unit->paths.stamp.profile = sp_fs_join_path(session->mem, unit->paths.stamp.dir, SP_LIT("profile.stamp"));

  sp_fs_create_dir(unit->paths.stamp.dir);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t identity = spn_profile_identity_to_str(scratch.mem, &build->profile);
  sp_str_t existing = sp_zero;
  sp_io_read_file(scratch.mem, unit->paths.stamp.profile, &existing);
  if (!sp_str_equal(existing, identity)) {
    sp_fs_create_file_str(unit->paths.stamp.profile, identity);
  }
  sp_mem_end_scratch(scratch);

  return unit;
}
