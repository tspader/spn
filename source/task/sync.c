#include "sp.h"
#include "sp/str.h"
#include "app/types.h"
#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "compiler/driver.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/event.h"
#include "forward/types.h"
#include "git/cache.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/session.h"
#include "session/types.h"
#include "spn.h"
#include "task/task.h"
#include "task/types.h"
#include "toml/loader.h"
#include "toml/issue.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "unit/types.h"

SP_PRIVATE spn_toolchain_unit_t*
setup_toolchain_unit(spn_session_t* session, spn_toolchain_store_t* store, spn_toolchain_t* toolchain) {
  spn_toolchain_unit_t *unit = sp_alloc_type(spn.mem, spn_toolchain_unit_t);
  unit->session = session;
  unit->toolchain = toolchain;

  sp_str_t name = toolchain->name;

  struct { sp_str_t build; sp_str_t test; sp_str_t jsonl; } paths = {
    .build = sp_fs_join_path(spn.mem, spn.paths.log, sp_fmt(spn.mem, "toolchain.{}.build.log", sp_fmt_str(name)).value),
    .test = sp_fs_join_path(spn.mem, spn.paths.log, sp_fmt(spn.mem, "toolchain.{}.test.log", sp_fmt_str(name)).value),
    .jsonl = sp_fs_join_path(spn.mem, spn.paths.log, sp_fmt(spn.mem, "toolchain.{}.jsonl", sp_fmt_str(name)).value),
  };

  spn_lazy_log_init(&unit->logs.build, paths.build);
  spn_lazy_log_init(&unit->logs.test, paths.test);
  spn_lazy_log_init(&unit->logs.jsonl, paths.jsonl);

  sp_tm_timer_t timer = sp_tm_start_timer();

  bool cached = true;
  sp_str_t url = sp_zero;
  if (!sp_opt_is_null(toolchain->artifact)) {
    spn_artifact_t artifact = sp_opt_get(toolchain->artifact);
    url = spn_artifact_resolve_url(spn.mem, artifact, store->mirror);
    cached = sp_fs_is_dir(spn_toolchain_store_path(store, artifact));
  }

  if (!cached) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC,
      .sync = {
        .name = name,
        .url = url,
      }});
  }

  spn_err_union_t err = spn_toolchain_provision(store, toolchain, &unit->root);
  if (err.kind) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = err,
    });
    return SP_NULLPTR;
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t){
    .kind = SPN_EVENT_SYNC_PACKAGE,
    .sync_pkg = {
      .name = name,
      .url = url,
      .source_path = unit->root,
      .time = sp_tm_read_timer(&timer),
      .fetched = !cached,
    }
  });

  if (sp_str_empty(unit->root)) {
    unit->compiler = toolchain->compiler;
    unit->cxx = toolchain->cxx;
    unit->linker = toolchain->linker;
    unit->archiver = toolchain->archiver;
  } else {
    unit->compiler = spn_toolchain_launcher_with_root(spn.mem, toolchain->compiler, unit->root);
    unit->cxx = spn_toolchain_launcher_with_root(spn.mem, toolchain->cxx, unit->root);
    unit->linker = spn_toolchain_launcher_with_root(spn.mem, toolchain->linker, unit->root);
    unit->archiver = spn_toolchain_launcher_with_root(spn.mem, toolchain->archiver, unit->root);
  }

  if (sp_opt_is_null(toolchain->artifact)) {
    // @spader If you bring your own toolchain, I don't care about
    // tracking it for now.
    unit->identity = sp_hash_str(unit->compiler.program);
  }

  return unit;
}

static spn_err_t materialize_tree(spn_session_t* session, sp_str_t name, spn_pkg_tree_t tree, sp_str_t* root, bool* fetched) {
  switch (tree.kind) {
    case SPN_PKG_TREE_LOCAL: {
      *root = tree.local;
      return SPN_OK;
    }
    case SPN_PKG_TREE_GIT: {
      if (!spn_git_cache_is_checkout_cached(session->git, tree.git)) {
        spn_event_buffer_push(spn.events, (spn_build_event_t){
          .kind = SPN_EVENT_SYNC,
            .sync = {
              .name = name,
              .url = tree.git.url,
            }
        });
      }

      spn_git_checkout_t* checkout = SP_NULLPTR;
      if (spn_git_cache_ensure_checkout(session->git, tree.git, &checkout)) {
        sp_str_t error = sp_str_lit("failed to fetch repository");
        if (checkout && !sp_str_empty(checkout->error)) {
          error = checkout->error;
        }
        spn_event_buffer_push(spn.events, (spn_build_event_t) {
          .kind = SPN_EVENT_SYNC_FAILED,
          .sync_failed = {
              .name = name,
              .url = tree.git.url,
              .error = error,
          }
        });
        return SPN_ERROR;
      }

      *root = checkout->path;
      *fetched |= checkout->fetched;
      return SPN_OK;
    }
    case SPN_PKG_TREE_NONE: {
      *root = sp_str_lit("");
      return SPN_OK;
    }
  }

  sp_unreachable_return(SPN_ERROR);
}

static sp_str_t absolute_to(sp_str_t path, sp_str_t root) {
  return sp_fs_is_absolute(path) ? path : sp_fs_join_path(spn.mem, root, path);
}

static spn_target_info_t resolve_script(spn_target_info_t script, sp_str_t root) {
  spn_target_info_t resolved = script;
  resolved.source = sp_da_new(spn.mem, sp_str_t);
  resolved.include = sp_da_new(spn.mem, sp_str_t);
  sp_da_for(script.source, it) {
    sp_da_push(resolved.source, absolute_to(script.source[it], root));
  }
  sp_da_for(script.include, it) {
    sp_da_push(resolved.include, absolute_to(script.include[it], root));
  }
  return resolved;
}

static spn_err_t load_manifest(spn_session_t* session, sp_str_t name, sp_str_t path, spn_pkg_info_t** info) {
  spn_pkg_info_t* parsed = sp_alloc_type(spn.mem, spn_pkg_info_t);
  spn_toml_loader_t ctx = sp_zero;
  spn_toml_loader_init(&ctx, spn.mem, session->intern);
  if (spn_codegen_load_pkg(&ctx, path, parsed)) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_MANIFEST,
      .manifest_err = {
        .name = name,
        .path = path,
        .error = spn_codegen_issues_message(spn.mem, ctx.issues),
        .issues = ctx.issues,
      }});
    return SPN_ERROR;
  }

  // Short names only: published manifests routinely omit the namespace the
  // index assigns, but config keys and consumer routing use the name
  sp_str_t requested = spn_pkg_name_from_qualified(name).name;
  if (!sp_str_equal(parsed->name, requested)) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_MANIFEST,
      .manifest_err = {
        .name = name,
        .path = path,
        .error = sp_fmt(spn.mem, "the manifest declares {} but the release is {}", sp_fmt_str(parsed->name), sp_fmt_str(requested)).value,
      }});
    return SPN_ERROR;
  }

  *info = parsed;
  return SPN_OK;
}

static sp_str_t sync_url(spn_pkg_tree_t recipe, spn_pkg_tree_t source) {
  if (source.kind == SPN_PKG_TREE_GIT) {
    return source.git.url;
  }
  if (recipe.kind == SPN_PKG_TREE_GIT) {
    return recipe.git.url;
  }
  return sp_str_lit("");
}

static spn_err_t load_package(spn_session_t* session, spn_resolved_pkg_t* pkg, spn_loaded_pkg_t* loaded) {
  sp_tm_timer_t timer = sp_tm_start_timer();
  bool fetched = false;
  sp_str_t qualified = spn_intern_str(pkg->id.qualified);

  loaded->source = pkg->source;

  spn_try(materialize_tree(session, qualified, pkg->origin.recipe, &loaded->roots.recipe, &fetched));

  loaded->paths.manifest = sp_fs_join_path(spn.mem, loaded->roots.recipe, pkg->origin.paths.manifest);
  loaded->paths.script = sp_fs_join_path(spn.mem, loaded->roots.recipe, pkg->origin.paths.script);

  loaded->info = pkg->origin.info;
  if (!loaded->info) {
    spn_try(load_manifest(session, qualified, loaded->paths.manifest, &loaded->info));
  }

  loaded->configure = resolve_script(loaded->info->configure, loaded->roots.recipe);
  loaded->build = resolve_script(loaded->info->build, loaded->roots.recipe);

  // $ROOT/spn.c is the default script: when a package declares no split
  // configure script, the jammed file provides configure/package/node fns
  if (sp_da_empty(loaded->configure.source)) {
    sp_str_t candidates [] = {
      sp_fs_join_path(spn.mem, loaded->roots.recipe, sp_str_lit("configure.c")),
      loaded->paths.script,
    };
    sp_carr_for(candidates, it) {
      if (sp_fs_is_file(candidates[it])) {
        sp_da_push(loaded->configure.source, candidates[it]);
        break;
      }
    }
  }

  if (sp_da_empty(loaded->build.source)) {
    sp_str_t candidate = sp_fs_join_path(spn.mem, loaded->roots.recipe, sp_str_lit("build.c"));
    if (sp_fs_is_file(candidate)) {
      sp_da_push(loaded->build.source, candidate);
    }
  }

  if (pkg->origin.source.kind == SPN_PKG_TREE_NONE) {
    loaded->roots.source = loaded->roots.recipe;
  } else {
    spn_try(materialize_tree(session, qualified, pkg->origin.source, &loaded->roots.source, &fetched));
  }

  loaded->elapsed = sp_tm_read_timer(&timer);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_PACKAGE, .pkg = loaded->info,
    .sync_pkg = {
      .name = qualified,
      .url = sync_url(pkg->origin.recipe, pkg->origin.source),
      .source_path = loaded->roots.source,
      .time = loaded->elapsed,
      .fetched = fetched,
    }});

  return SPN_OK;
}

static s32 sync_package_node(spn_bg_cmd_t *cmd, void *user_data) {
  spn_sync_pkg_job_t* job = (spn_sync_pkg_job_t *)user_data;
  return load_package(job->session, job->pkg, &job->loaded);
}

static s32 sync_toolchain_node(spn_bg_cmd_t* cmd, void* user_data) {
  spn_sync_toolchain_job_t* job = (spn_sync_toolchain_job_t *)user_data;
  job->unit = setup_toolchain_unit(job->session, job->store, job->toolchain);
  return job->unit ? SPN_OK : SPN_ERROR;
}

static spn_pkg_unit_t* add_package_units(spn_session_t* s, spn_build_unit_t* build, spn_pkg_id_t id) {
  spn_pkg_unit_t* existing = spn_session_find_pkg_unit(s, build, id);
  if (existing) {
    return existing;
  }

  spn_resolved_pkg_t* pkg = sp_ht_getp(s->resolve, id);
  spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);
  sp_assert(pkg);
  sp_assert(loaded);

  spn_pkg_unit_t* unit = spn_session_add_pkg_unit(s, build, id, loaded);
  sp_da_for(pkg->edges, it) {
    sp_da_push(unit->deps, ((spn_pkg_dep_t) {
      .unit = add_package_units(s, build, pkg->edges[it].id),
      .kind = pkg->edges[it].kind,
      .private = pkg->edges[it].private,
    }));
  }
  return unit;
}

spn_err_union_t add_compilation_units(spn_session_t *s) {
  if (sp_da_empty(s->plan.builds)) {
    spn_build_unit_t* build = sp_alloc_type(s->mem, spn_build_unit_t);
    *build = (spn_build_unit_t) {
      .id = (spn_build_unit_id_t)sp_da_size(s->plan.builds),
      .kind = SPN_BUILD_KIND_TARGET,
      .profile = s->profile,
      .toolchain = s->units.toolchain,
      .paths = { .profile = s->paths.profile },
    };
    spn_build_plan_t plan = {
      .build = build,
      .selection = s->plan.request.targets,
    };
    sp_da_init(s->mem, plan.roots);
    sp_da_push(s->plan.builds, plan);
  }

  spn_pkg_id_t root = SP_ZERO_INITIALIZE();
  sp_ht_for_kv(s->resolve, it) {
    if (it.val->source == SPN_PKG_SOURCE_ROOT) {
      root = it.val->id;
      break;
    }
  }

  sp_assert(root.qualified);
  sp_da_for(s->plan.builds, it) {
    spn_build_plan_t* plan = &s->plan.builds[it];
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_cc_flags_t flags = SP_ZERO_INITIALIZE();
    spn_err_union_t err = spn_cc_render_flags(scratch.mem, plan->build->toolchain->toolchain->driver, &plan->build->profile, &flags);
    sp_mem_end_scratch(scratch);
    if (err.kind) {
      return err;
    }
    plan->root = (spn_pkg_unit_id_t) {
      .pkg = root,
      .ctx = plan->build->id,
    };
    add_package_units(s, plan->build, root);
  }
  return spn_result(SPN_OK);
}

spn_task_step_t spn_task_sync_packages_init(spn_app_t *app) {
  spn_session_t *session = &app->session;

  session->git = sp_alloc_type(spn.mem, spn_git_cache_t);
  spn_git_cache_init(session->git, spn.mem, session->intern, spn.paths.caches.git.dir);

  app->sync.store = (spn_toolchain_store_t){
    .mem = spn.mem,
    .dir = spn.paths.toolchain,
    .mirror = sp_env_get(spn.env, sp_str_lit("SPN_MIRROR")),
    .fetch = spn_fetch_curl,
  };

  spn_build_graph_t *graph = &session->sync.graph;
  spn_bg_init(graph, spn.mem);

  sp_da_init(spn.mem, app->sync.packages);
  sp_da_init(spn.mem, app->sync.toolchains);

  u32 num_index = 0;
  u32 num_file = 0;
  sp_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t *pkg = it.val;
    switch (pkg->source) {
      case SPN_PKG_SOURCE_ROOT: break;
      case SPN_PKG_SOURCE_INDEX: num_index++; break;
      case SPN_PKG_SOURCE_FILE: num_file++; break;
    }

    spn_sync_pkg_job_t *job = sp_alloc_type(spn.mem, spn_sync_pkg_job_t);
    job->session = session;
    job->pkg = pkg;
    sp_da_push(app->sync.packages, job);

    spn_bg_add_fn(graph, sync_package_node, job);
  }

  sp_da_for(session->selection.required, it) {
    spn_sync_toolchain_job_t *job = sp_alloc_type(spn.mem, spn_sync_toolchain_job_t);
    job->session = session;
    job->store = &app->sync.store;
    job->toolchain = session->selection.required[it];
    sp_da_push(app->sync.toolchains, job);

    spn_bg_add_fn(graph, sync_toolchain_node, job);
  }

  spn_event_buffer_push( spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_START,
    .sync_start = {
      .num_packages = sp_da_size(app->sync.packages),
      .num_index = num_index,
      .num_file = num_file,
    }});

  session->sync.dirty = spn_bg_compute_forced_dirty(graph);
  session->sync.executor = spn_bg_executor_new(graph, session->sync.dirty, (spn_bg_executor_config_t){
    .num_threads = 16,
  });
  spn_bg_executor_run(session->sync.executor);

  return spn_task_continue();
}

spn_task_step_t spn_task_sync_packages_update(spn_app_t *app) {
  spn_session_t *session = &app->session;

  if (!sp_atomic_s32_get(&session->sync.executor->shutdown)) {
    return spn_task_continue();
  }

  spn_bg_executor_join(session->sync.executor);
  if (!sp_da_empty(session->sync.executor->errors)) {
    return spn_task_fail(SPN_ERROR);
  }

  sp_da_for(app->sync.packages, it) {
    spn_sync_pkg_job_t *job = app->sync.packages[it];
    job->pkg->name = job->loaded.info->name;
    job->pkg->options = job->loaded.info->options;
    sp_ht_insert(session->packages, job->pkg->id, job->loaded);
  }

  if (spn_session_apply_options(session)) {
    return spn_task_fail(SPN_ERROR);
  }

  if (session->gates.reresolve) {
    session->gates.reresolve = false;
    if (!spn_task_rewind(&app->tasks, SPN_TASK_RESOLVE)) {
      return spn_task_fail(SPN_ERROR);
    }
    return spn_task_continue();
  }

  session->units.toolchains = sp_da_new(session->mem, spn_toolchain_unit_t *);
  sp_da_init(session->mem, session->units.compile_commands);
  sp_da_init(session->mem, session->plan.builds);
  sp_da_for(app->sync.toolchains, it) {
    spn_sync_toolchain_job_t* job = app->sync.toolchains[it];
    sp_da_push(session->units.toolchains, job->unit);
    if (job->toolchain == session->selection.build) {
      session->units.toolchain = job->unit;
    }
    if (job->toolchain == session->selection.script) {
      session->units.script = job->unit;
    }
  }

  spn_err_union_t flags_err = add_compilation_units(session);
  if (!flags_err.kind) {
    flags_err = spn_task_create_script_units(session);
  }
  if (flags_err.kind) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = flags_err,
    });
    return spn_task_fail(SPN_ERROR);
  }

  sp_env_t *env = &session->env;
  sp_env_init(session->mem, env);
  sp_env_insert(env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->compiler));
  sp_env_insert(env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->archiver));
  sp_env_insert(env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->linker));
  if (spn_toolchain_has_cxx(session->units.toolchain->toolchain)) {
    sp_env_insert(env, sp_str_lit("CXX"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->cxx));
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_END,
    .sync_end = {
      .num_synced = sp_da_size(app->sync.packages),
      .time = session->sync.executor->elapsed,
    }});

  return spn_task_done();
}
