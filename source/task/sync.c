#include "app/types.h"
#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/event.h"
#include "forward/types.h"
#include "git/cache.h"
#include "index/types.h"
#include "log/lazy/lazy.h"
#include "pkg/id.h"
#include "pkg/load.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/session.h"
#include "session/types.h"
#include "spn.h"
#include "task/task.h"
#include "task/types.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "unit/types.h"

SP_PRIVATE spn_toolchain_unit_t *
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

  return unit;
}

static spn_pkg_tree_t tree_local(sp_str_t path) {
  return (spn_pkg_tree_t){
    .kind = SPN_PKG_TREE_LOCAL,
    .local = path
  };
}

static spn_pkg_tree_t tree_git(spn_index_rel_source_t source) {
  return (spn_pkg_tree_t){
      .kind = SPN_PKG_TREE_GIT,
      .git = { .url = source.url, .rev = source.rev, .dir = source.dir },
  };
}

static spn_err_t materialize_tree(spn_session_t *session, sp_str_t name,
                                  spn_pkg_tree_t tree, sp_str_t *root,
                                  bool *fetched) {
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

    spn_git_checkout_t *checkout = SP_NULLPTR;
    if (spn_git_cache_ensure_checkout(session->git, tree.git, &checkout)) {
      sp_str_t error = sp_str_lit("failed to fetch repository");
      if (checkout && !sp_str_empty(checkout->error)) {
        error = checkout->error;
      }
      spn_event_buffer_push(spn.events,
        (spn_build_event_t){.kind = SPN_EVENT_SYNC_FAILED,
          .sync_failed = {
              .name = name,
              .url = tree.git.url,
              .error = error,
          }});
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

static sp_str_t resolve_script(spn_target_info_t* script, sp_str_t root) {
  sp_da_for(script->source, it) {
    if (!sp_fs_is_absolute(script->source[it])) {
      script->source[it] = sp_fs_join_path(spn.mem, root, script->source[it]);
    }
  }
  sp_da_for(script->include, it) {
    if (!sp_fs_is_absolute(script->include[it])) {
      script->include[it] = sp_fs_join_path(spn.mem, root, script->include[it]);
    }
  }
  return sp_da_empty(script->source) ? sp_str_lit("") : script->source[0];
}

// @spader this is junk
static spn_err_t load_package(
  spn_session_t *session, spn_loaded_pkg_t *loaded,
  sp_str_t name,
  spn_pkg_info_t *info,
  spn_pkg_tree_t recipe_tree,
  spn_index_rel_paths_t rel, bool derive_source,
  spn_pkg_tree_t source_tree
) {
  sp_tm_timer_t timer = sp_tm_start_timer();
  bool fetched = false;

  spn_try(materialize_tree(session, name, recipe_tree, &loaded->roots.recipe, &fetched));

  loaded->paths.manifest = sp_fs_join_path(spn.mem, loaded->roots.recipe, rel.manifest);
  loaded->paths.script = sp_fs_join_path(spn.mem, loaded->roots.recipe, rel.script);

  if (!info) {
    info = sp_alloc_type(spn.mem, spn_pkg_info_t);
    spn_codegen_ctx_t ctx = sp_zero;
    spn_codegen_ctx_init(&ctx, spn.mem, session->intern);
    if (spn_codegen_load_pkg(&ctx, loaded->paths.manifest, info)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_MANIFEST,
        .manifest_err = {
          .name = name,
          .path = loaded->paths.manifest,
          .error = spn_codegen_issues_message(spn.mem, ctx.issues),
          .issues = ctx.issues,
        }});
      return SPN_ERROR;
    }
  }

  loaded->info = info;
  loaded->paths.configure = resolve_script(&info->configure, loaded->roots.recipe);
  loaded->paths.build = resolve_script(&info->build, loaded->roots.recipe);

  // $ROOT/spn.c is the default script: when a package declares no split
  // configure script, the jammed file provides configure/package/node fns
  if (!sp_fs_is_file(loaded->paths.configure) && sp_fs_is_file(loaded->paths.script)) {
    info->configure.source[0] = loaded->paths.script;
    loaded->paths.configure = loaded->paths.script;
  }

  if (derive_source) {
    source_tree = spn_pkg_manifest_source_tree(info);
  }

  if (source_tree.kind == SPN_PKG_TREE_NONE) {
    loaded->roots.source = loaded->roots.recipe;
  } else {
    spn_try(materialize_tree(session, name, source_tree, &loaded->roots.source,
                             &fetched));
  }

  loaded->elapsed = sp_tm_read_timer(&timer);

  sp_str_t url = sp_str_lit("");
  if (source_tree.kind == SPN_PKG_TREE_GIT) {
    url = source_tree.git.url;
  } else if (recipe_tree.kind == SPN_PKG_TREE_GIT) {
    url = recipe_tree.git.url;
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_PACKAGE, .pkg = info,
    .sync_pkg = {
      .name = name,
      .url = url,
      .source_path = loaded->roots.source,
      .time = loaded->elapsed,
      .fetched = fetched,
    }});

  return SPN_OK;
}

// While developing a recipe you don't want to publish on every edit, so
// SPN_PATCH_DIR/<name> stands in for the manifest repo. It's a plain tree
// substitution: the recipe comes from disk, the source is derived from that
// local manifest just like any other local recipe.
static sp_str_t patch_dir(spn_session_t *session, spn_index_rel_t *release) {
  sp_str_t patches = sp_env_get(spn.env, sp_str_lit("SPN_PATCH_DIR"));
  if (sp_str_empty(patches))
    return sp_str_lit("");

  sp_str_t dir = sp_fs_join_path(spn.mem, patches, release->id.name);
  sp_str_t manifest = sp_fs_join_path(spn.mem, dir, release->paths.manifest);
  if (!sp_fs_exists(manifest))
    return sp_str_lit("");

  return dir;
}

static spn_index_rel_paths_t local_rel_paths(sp_str_t manifest) {
  sp_str_t name = sp_str_strip_left(manifest, sp_fs_parent_path(manifest));
  return (spn_index_rel_paths_t){
      .manifest = sp_str_strip_left(name, sp_str_lit("/")),
      .script = sp_str_lit("spn.c"),
  };
}

static spn_err_t load_index_package(spn_session_t *session,
                                    spn_resolved_pkg_t *resolved,
                                    spn_loaded_pkg_t *loaded) {
  spn_index_rel_t *release = resolved->index.release;

  sp_str_t patch = patch_dir(session, release);
  if (!sp_str_empty(patch)) {
    return load_package(session, loaded, resolved->qualified, SP_NULLPTR,
                        tree_local(patch), release->paths, true,
                        sp_zero_s(spn_pkg_tree_t));
  }

  // @spader this is junk
  // The recipe lives in the manifest repo if there is one, otherwise the
  // source repo carries it. The source is whatever the release pinned.
  spn_pkg_tree_t recipe_tree = !sp_str_empty(release->manifest.url)
                                   ? tree_git(release->manifest)
                                   : tree_git(release->source);
  spn_pkg_tree_t source_tree =
      !sp_str_empty(release->source.url)
          ? tree_git(release->source)
          : (spn_pkg_tree_t){.kind = SPN_PKG_TREE_NONE};

  return load_package(session, loaded, resolved->qualified, SP_NULLPTR,
                      recipe_tree, release->paths, false, source_tree);
}

static spn_err_t load_file_package(spn_session_t *session, spn_resolved_pkg_t *pkg, spn_loaded_pkg_t *loaded) {
  spn_registry_pkg_t *entry = sp_ht_getp(session->registry, spn_pkg_id(session->intern, pkg->qualified));
  sp_assert(entry);

  sp_str_t dir = sp_fs_parent_path(entry->manifest);
  return load_package(session, loaded, pkg->qualified, entry->info,
                      tree_local(dir), local_rel_paths(entry->manifest), true,
                      sp_zero_s(spn_pkg_tree_t));
}

static spn_err_t load_root_package(spn_session_t *session, spn_loaded_pkg_t *loaded) {
  return load_package(session, loaded, session->pkg->qualified, session->pkg,
                      tree_local(spn.paths.project),
                      local_rel_paths(spn.paths.manifest), false,
                      sp_zero_s(spn_pkg_tree_t));
}

static s32 sync_package_node(spn_bg_cmd_t *cmd, void *user_data) {
  spn_sync_pkg_job_t* job = (spn_sync_pkg_job_t *)user_data;
  spn_session_t* session = job->session;
  spn_resolved_pkg_t* pkg = job->pkg;

  switch (pkg->source) {
    case SPN_PKG_SOURCE_ROOT: return load_root_package(session, &job->loaded);
    case SPN_PKG_SOURCE_INDEX: return load_index_package(session, pkg, &job->loaded);
    case SPN_PKG_SOURCE_FILE: return load_file_package(session, pkg, &job->loaded);
  }

  sp_unreachable_return(SPN_ERROR);
}

static s32 sync_toolchain_node(spn_bg_cmd_t *cmd, void *user_data) {
  spn_sync_toolchain_job_t *job = (spn_sync_toolchain_job_t *)user_data;
  job->unit = setup_toolchain_unit(job->session, job->store, job->toolchain);
  return job->unit ? SPN_OK : SPN_ERROR;
}

void add_compilation_units(spn_session_t *session, spn_resolver_t *resolver) {
  sp_ht_for_kv(session->packages, it) {
    spn_session_add_pkg(session, it.val);
  }

  sp_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* pkg = it.val;
    spn_pkg_unit_t* unit = spn_session_find_pkg_by_id(session, pkg->id);

    sp_da(spn_pkg_unit_t *) deps = sp_da_new(session->mem, spn_pkg_unit_t *);
    sp_da_for(pkg->deps, j) {
      spn_pkg_unit_t* dep = spn_session_find_pkg_by_qualified(session, pkg->deps[j].qualified);
      sp_da_push(deps, dep);
    }

    sp_om_insert(session->units.graph, unit->id, deps);
  }
}

spn_task_result_t spn_task_sync_init(spn_app_t *app) {
  spn_session_t *session = &app->session;

  session->git = sp_alloc_type(spn.mem, spn_git_cache_t);
  spn_git_cache_init(session->git, spn.mem, session->intern, spn.paths.source);

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
    case SPN_PKG_SOURCE_ROOT:
      break;
    case SPN_PKG_SOURCE_INDEX:
      num_index++;
      break;
    case SPN_PKG_SOURCE_FILE:
      num_file++;
      break;
    }

    spn_sync_pkg_job_t *job = sp_alloc_type(spn.mem, spn_sync_pkg_job_t);
    job->session = session;
    job->pkg = pkg;
    job->loaded.source = pkg->source;
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

  return SPN_TASK_CONTINUE;
}

spn_task_result_t spn_task_sync_update(spn_app_t *app) {
  spn_session_t *session = &app->session;

  if (!sp_atomic_s32_get(&session->sync.executor->shutdown)) {
    return SPN_TASK_CONTINUE;
  }

  spn_bg_executor_join(session->sync.executor);
  if (!sp_da_empty(session->sync.executor->errors)) {
    return SPN_TASK_ERROR;
  }

  sp_da_for(app->sync.packages, it) {
    spn_sync_pkg_job_t *job = app->sync.packages[it];
    sp_ht_insert(session->packages, job->pkg->id, job->loaded);
  }

  session->units.toolchains = sp_da_new(session->mem, spn_toolchain_unit_t *);
  sp_da_for(app->sync.toolchains, it) {
    spn_sync_toolchain_job_t *job = app->sync.toolchains[it];
    sp_da_push(session->units.toolchains, job->unit);
    if (job->toolchain == session->selection.build) {
      session->units.toolchain = job->unit;
    }
    if (job->toolchain == session->selection.script) {
      session->units.script = job->unit;
    }
  }

  add_compilation_units(session, app->resolver);

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

  return SPN_TASK_DONE;
}
