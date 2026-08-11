#include "sp.h"
#include "sp/str.h"
#include "ctx/ctx.h"
#include "ctx/types.h"
#include "dag/dag.h"
#include "error/error.h"
#include "error/types.h"
#include "event/event.h"
#include "core/core.h"
#include "core/types.h"
#include "git/cache.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
#include "op/op.h"
#include "pkg/id.h"
#include "pkg/load.h"
#include "pkg/options.h"
#include "pkg/patch.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/session.h"
#include "session/types.h"
#include "spn/core.h"
#include "thread_pool/thread_pool.h"
#include "toml/loader.h"
#include "toml/issue.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "unit/types.h"
#include "when/when.h"
#include "external/wasm/wasm.h"

typedef struct {
  spn_op_t* op;
  spn_session_t* session;
  spn_resolved_pkg_t* pkg;
  spn_loaded_pkg_t loaded;
  spn_err_t err;
} pkg_job_t;

typedef struct {
  spn_op_t* op;
  spn_toolchain_unit_t* unit;
  spn_err_t err;
} toolchain_job_t;

static spn_err_t setup_toolchain_unit(spn_toolchain_store_t* store, spn_toolchain_unit_t* unit) {
  spn_toolchain_info_t* toolchain = unit->info;
  sp_str_t name = toolchain->name;

  sp_tm_timer_t timer = sp_tm_start_timer();

  bool cached = true;
  sp_str_t url = sp_zero;
  if (toolchain->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION) {
    spn_artifact_t artifact = sp_opt_get(unit->artifact);
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

  spn_try(spn_err_emit(&spn, spn_toolchain_provision(store, toolchain, unit->artifact, &unit->root)));

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

  unit->cc = (spn_cc_toolchain_t) {
    .name = toolchain->name,
    .driver = toolchain->driver,
    .archiver_driver = toolchain->driver == SPN_CC_DRIVER_MSVC ? SPN_AR_DRIVER_MSVC : SPN_AR_DRIVER_GNU,
  };
  if (sp_str_empty(unit->root)) {
    unit->cc.compiler = toolchain->compiler;
    unit->cc.cxx = toolchain->cxx;
    unit->cc.linker = toolchain->linker;
    unit->cc.archiver = toolchain->archiver;
  } else {
    unit->cc.compiler = spn_toolchain_launcher_with_root(spn.mem, toolchain->compiler, unit->root);
    unit->cc.cxx = spn_toolchain_launcher_with_root(spn.mem, toolchain->cxx, unit->root);
    unit->cc.linker = spn_toolchain_launcher_with_root(spn.mem, toolchain->linker, unit->root);
    unit->cc.archiver = spn_toolchain_launcher_with_root(spn.mem, toolchain->archiver, unit->root);
  }

  if (toolchain->source == SPN_TOOLCHAIN_SOURCE_LOCAL) {
    unit->identity = sp_hash_str(unit->cc.compiler.program);
  }

  return SPN_OK;
}

static spn_err_t materialize_tree(spn_session_t* session, sp_str_t name, spn_pkg_root_t tree, sp_str_t* root, bool* fetched) {
  switch (tree.kind) {
    case SPN_PKG_ROOT_LOCAL: {
      *root = tree.local;
      return SPN_OK;
    }
    case SPN_PKG_ROOT_GIT: {
      if (!spn_git_cache_is_checkout_cached(&session->ctx->caches.git, tree.git)) {
        spn_event_buffer_push(spn.events, (spn_build_event_t){
          .kind = SPN_EVENT_SYNC,
          .sync = {
            .name = name,
            .url = tree.git.url,
          }
        });
      }

      spn_git_checkout_t* checkout = SP_NULLPTR;
      if (spn_git_cache_ensure_checkout(&session->ctx->caches.git, tree.git, &checkout)) {
        sp_str_t error = sp_str_lit("failed to fetch repository");
        if (!sp_str_empty(checkout->error)) {
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
    case SPN_PKG_ROOT_NONE: {
      *root = sp_str_lit("");
      return SPN_OK;
    }
  }

  sp_unreachable_return(SPN_ERROR);
}

static sp_da(spn_tree_path_t) resolve_paths(spn_gated_path_list_t entries, spn_loaded_pkg_t* loaded, spn_when_env_t* env) {
  sp_da(spn_tree_path_t) resolved = sp_da_new(spn.mem, spn_tree_path_t);
  sp_da_for(entries, it) {
    if (!spn_when_eval(&entries[it].when, env)) {
      continue;
    }
    spn_tree_path_t entry = { .path = entries[it].path, .tree = entries[it].tree };
    sp_da_push(resolved, ((spn_tree_path_t) {
      .path = spn_tree_path_resolve(spn.mem, loaded->roots, entry),
      .tree = entry.tree,
    }));
  }
  return resolved;
}

static spn_err_t configure_source_err(spn_ctx_t* ctx, sp_str_t name, sp_str_t source) {
  return spn_err_emit(ctx, (spn_err_union_t) {
    .kind = SPN_ERR_CONFIGURE_SOURCE,
    .configure_source = {
      .name = name,
      .source = source,
    }});
}

static spn_err_t resolve_configure_source(spn_ctx_t* ctx, sp_str_t name, spn_gated_path_list_t declared, spn_loaded_pkg_t* loaded, spn_when_env_t* env, sp_da(spn_tree_path_t)* source) {
  sp_da(spn_tree_path_t) resolved = sp_da_new(spn.mem, spn_tree_path_t);
  sp_da_for(declared, it) {
    if (!spn_when_eval(&declared[it].when, env)) {
      continue;
    }
    spn_tree_path_t entry = { .path = declared[it].path, .tree = declared[it].tree };
    sp_str_t root = spn_tree_root(loaded->roots, entry.tree);
    if (!sp_fs_is_glob(entry.path)) {
      sp_str_t path = spn_tree_path_resolve(spn.mem, loaded->roots, entry);
      if (!sp_fs_is_target_file(path)) {
        return configure_source_err(ctx, name, entry.path);
      }
      sp_da_push(resolved, ((spn_tree_path_t) { .path = path, .tree = entry.tree }));
      continue;
    }

    sp_da(spn_dag_match_t) matches = sp_da_new(spn.mem, spn_dag_match_t);
    if (spn_dag_glob(spn.mem, root, entry.path, SP_NULLPTR, &matches) || sp_da_empty(matches)) {
      return configure_source_err(ctx, name, entry.path);
    }
    sp_da_for(matches, jt) {
      sp_da_push(resolved, ((spn_tree_path_t) { .path = matches[jt].path, .tree = entry.tree }));
    }
  }
  *source = resolved;
  return SPN_OK;
}

static sp_da(spn_tree_path_t) detect_configure_source(spn_loaded_pkg_t* loaded) {
  sp_da(spn_tree_path_t) source = sp_da_new(spn.mem, spn_tree_path_t);
  sp_str_t candidates [] = {
    sp_fs_join_path(spn.mem, loaded->roots.recipe, sp_str_lit("configure.c")),
    loaded->paths.script,
  };
  sp_carr_for(candidates, it) {
    if (sp_fs_is_target_file(candidates[it])) {
      sp_da_push(source, ((spn_tree_path_t) { .path = candidates[it], .tree = SPN_TREE_MANIFEST }));
      break;
    }
  }
  return source;
}

static spn_err_t load_manifest(spn_session_t* session, sp_str_t name, sp_str_t path, spn_pkg_info_t** info) {
  spn_pkg_info_t* parsed = sp_alloc_type(spn.mem, spn_pkg_info_t);
  spn_try(spn_err_emit(session->ctx, spn_pkg_load(spn.mem, session->ctx->intern, path, SPN_MANIFEST_DEP, name, parsed)));

  // Short names only: published manifests routinely omit the namespace the
  // index assigns, but config keys and consumer routing use the name
  sp_str_t requested = spn_pkg_name_from_qualified(name).name;
  if (!sp_str_equal(parsed->name, requested)) {
    return spn_err_emit(session->ctx, (spn_err_union_t) {
      .kind = SPN_ERR_PKG_MISMATCH,
      .mismatch = { .path = path, .declared = parsed->name, .requested = name },
    });
  }

  *info = parsed;
  return SPN_OK;
}

static sp_str_t sync_url(spn_pkg_root_t recipe, spn_pkg_root_t source) {
  if (source.kind == SPN_PKG_ROOT_GIT) {
    return source.git.url;
  }
  if (recipe.kind == SPN_PKG_ROOT_GIT) {
    return recipe.git.url;
  }
  return sp_str_lit("");
}

static bool package_has_build_deps(spn_resolved_pkg_t* pkg) {
  sp_da_for(pkg->edges, it) {
    if (pkg->edges[it].kind == SPN_DEP_KIND_BUILD) {
      return true;
    }
  }
  return false;
}

static spn_err_t stamp_patches(spn_session_t* session, spn_resolved_pkg_t* pkg, sp_str_t qualified) {
  switch (spn_pkg_patch_stamp(session->pkg->patches, qualified, &pkg->origin.source)) {
    case SPN_PKG_PATCH_STAMP_NONE: {
      return SPN_OK;
    }
    case SPN_PKG_PATCH_STAMP_APPLIED: {
      if (!spn_git_cache_is_checkout_cached(&session->ctx->caches.git, pkg->origin.source.git)) {
        spn_event_buffer_push(spn.events, (spn_build_event_t) {
          .kind = SPN_EVENT_SYNC_PATCH,
          .sync = {
            .name = qualified,
            .url = pkg->origin.source.git.url,
          }});
      }
      return SPN_OK;
    }
    case SPN_PKG_PATCH_STAMP_NOT_GIT: {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_PATCH,
        .patch_err = {
          .name = qualified,
          .kind = SPN_PATCH_ERR_NOT_GIT,
        }});
      return SPN_ERROR;
    }
  }

  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t load_package(spn_session_t* session, spn_resolved_pkg_t* pkg, spn_loaded_pkg_t* loaded) {
  sp_tm_timer_t timer = sp_tm_start_timer();
  bool fetched = false;
  sp_str_t qualified = spn_intern_str(pkg->id.qualified);

  loaded->source = pkg->source;

  spn_try(stamp_patches(session, pkg, qualified));

  spn_try(materialize_tree(session, qualified, pkg->origin.recipe, &loaded->roots.recipe, &fetched));

  loaded->paths.manifest = sp_fs_join_path(spn.mem, loaded->roots.recipe, pkg->origin.paths.manifest);
  loaded->paths.script = sp_fs_join_path(spn.mem, loaded->roots.recipe, pkg->origin.paths.script);

  loaded->info = pkg->origin.info;
  if (!loaded->info) {
    spn_try(load_manifest(session, qualified, loaded->paths.manifest, &loaded->info));
  }

  if (pkg->origin.source.kind == SPN_PKG_ROOT_NONE) {
    loaded->roots.source = loaded->roots.recipe;
  } else {
    spn_try(materialize_tree(session, qualified, pkg->origin.source, &loaded->roots.source, &fetched));
  }

  spn_when_env_t facts = sp_zero;
  spn_when_env_from_profile(spn.mem, &session->profile, &facts);

  loaded->build = loaded->info->build;
  loaded->build.source = resolve_paths(loaded->info->build.gated.source, loaded, &facts);
  loaded->build.include = resolve_paths(loaded->info->build.gated.include, loaded, &facts);

  loaded->configure = loaded->info->configure;
  loaded->configure.include = resolve_paths(loaded->info->configure.gated.include, loaded, &facts);

  // @spader This is a weird case. Normally, a manifest is validated when we
  // actually load the TOML, mechanically. No real context needed. But lists
  // of sources can include globs, and those can hit files that are code
  // generated by part of your build, so those are validated at the last
  // moment, when we're ready to compile the sources that the glob lists.
  //
  // But for your configure script, if it uses a glob that produces no sources,
  // that needs to error. There's no point at which you could code generate
  // something that matches, so we can do it eagerly. And we *want* to do it
  // up front, so we fail fast.
  //
  // But...this isn't really the right place to do it. This code is supposed
  // to be more mechanical; "get the sources on disk, do basic validation". But
  // what I just described belongs in the graph layer.
  if (sp_da_empty(loaded->info->configure.gated.source)) {
    // @spader We need to stop doing this and force people to be explicit.
    // Instead of this weird detection, just make if so if you don't have
    // [package.configure] then you don't have a configure script. Simple.
    loaded->configure.source = detect_configure_source(loaded);
  } else {
    spn_try(resolve_configure_source(session->ctx, qualified, loaded->info->configure.gated.source, loaded, &facts, &loaded->configure.source));
  }

  if (sp_da_empty(loaded->build.source)) {
    sp_str_t candidate = sp_fs_join_path(spn.mem, loaded->roots.recipe, sp_str_lit("build.c"));
    if (sp_fs_is_target_file(candidate)) {
      sp_da_push(loaded->build.source, ((spn_tree_path_t) { .path = candidate, .tree = SPN_TREE_MANIFEST }));
    }
    else if (package_has_build_deps(pkg) && sp_fs_is_target_file(loaded->paths.script)) {
      sp_da_push(loaded->build.source, ((spn_tree_path_t) { .path = loaded->paths.script, .tree = SPN_TREE_MANIFEST }));
    }
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

static sp_atomic_s32_t sync_failed;

static void sync_package_node(void *data) {
  pkg_job_t* job = (pkg_job_t*)data;
  if (sp_atomic_s32_load(&sync_failed, SP_ATOMIC_SEQ_CST) || spn_op_cancelled(job->op)) {
    return;
  }
  job->err = load_package(job->session, job->pkg, &job->loaded);
  if (job->err) {
    sp_atomic_s32_store(&sync_failed, (s32)job->err, SP_ATOMIC_SEQ_CST);
  }
}

static void sync_toolchain_node(void* data) {
  toolchain_job_t* job = (toolchain_job_t*)data;
  if (sp_atomic_s32_load(&sync_failed, SP_ATOMIC_SEQ_CST) || spn_op_cancelled(job->op)) {
    return;
  }
  job->err = setup_toolchain_unit(&spn.caches.toolchains, job->unit);
  if (job->err) {
    sp_atomic_s32_store(&sync_failed, (s32)job->err, SP_ATOMIC_SEQ_CST);
  }
}

static spn_err_t check_unused_patches(spn_session_t* session) {
  spn_err_t err = SPN_OK;
  sp_da_for(session->pkg->patches, it) {
    sp_str_t qualified = session->pkg->patches[it].qualified;

    bool used = false;
    sp_ht_for_kv(session->resolve, jt) {
      if (sp_str_equal(spn_intern_str(jt.key->qualified), qualified)) {
        used = true;
        break;
      }
    }
    if (used) {
      continue;
    }

    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_PATCH,
      .patch_err = {
        .name = qualified,
        .kind = SPN_PATCH_ERR_UNUSED,
      }});
    err = SPN_ERROR;
  }
  return err;
}

spn_err_union_t sync(spn_op_t* op, bool* reresolve) {
  spn_session_t* session = op->session;
  sp_da(pkg_job_t*) packages = sp_da_new(session->mem, pkg_job_t*);
  sp_da(toolchain_job_t*) toolchains = sp_da_new(session->mem, toolchain_job_t*);

  u32 num_index = 0;
  u32 num_file = 0;
  sp_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t *pkg = it.val;
    switch (pkg->source) {
      case SPN_PKG_SOURCE_ROOT: break;
      case SPN_PKG_SOURCE_INDEX: num_index++; break;
      case SPN_PKG_SOURCE_FILE: num_file++; break;
    }

    pkg_job_t* job = sp_alloc_type(session->mem, pkg_job_t);
    job->op = op;
    job->session = session;
    job->pkg = pkg;
    sp_da_push(packages, job);
  }

  sp_da_for(session->units.toolchains, it) {
    toolchain_job_t* job = sp_alloc_type(session->mem, toolchain_job_t);
    job->op = op;
    job->unit = session->units.toolchains[it];
    sp_da_push(toolchains, job);
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_START,
    .sync_start = {
      .num_packages = sp_da_size(packages),
      .num_index = num_index,
      .num_file = num_file,
    }});

  sp_atomic_s32_store(&sync_failed, 0, SP_ATOMIC_SEQ_CST);

  u32 num_jobs = (u32)(sp_da_size(packages) + sp_da_size(toolchains));
  spn_thread_pool_t pool = sp_zero;
  spn_thread_pool_init(&pool, spn.mem, (spn_thread_pool_config_t) {
    .workers = sp_min(8, num_jobs),
    .on_worker_exit = spn_wasm_thread_exit,
  });

  sp_tm_timer_t timer = sp_tm_start_timer();

  sp_da_for(packages, it) {
    spn_thread_pool_submit(&pool.executor, (spn_thread_pool_job_t) { .fn = sync_package_node, .data = packages[it] });
  }
  sp_da_for(toolchains, it) {
    spn_thread_pool_submit(&pool.executor, (spn_thread_pool_job_t) { .fn = sync_toolchain_node, .data = toolchains[it] });
  }

  spn_thread_pool_wait(&pool);
  spn_thread_pool_deinit(&pool);
  u64 elapsed = sp_tm_read_timer(&timer);

  if (spn_op_cancelled(op)) {
    return spn_err_reported(SPN_ERR_CANCELLED);
  }

  sp_da_for(packages, it) {
    if (packages[it]->err) {
      return spn_err_reported(packages[it]->err);
    }
  }
  sp_da_for(toolchains, it) {
    if (toolchains[it]->err) {
      return spn_err_reported(toolchains[it]->err);
    }
  }

  sp_da_for(packages, it) {
    pkg_job_t* job = packages[it];
    job->pkg->name = job->loaded.info->name;
    job->pkg->options = job->loaded.info->options;
    sp_ht_insert(session->packages, job->pkg->id, job->loaded);
  }

  spn_try_union(spn_session_apply_options(session, reresolve));

  if (*reresolve) {
    return spn_result(SPN_OK);
  }

  spn_session_export_toolchain_env(session);
  spn_try_union(spn_session_validate_flags(session));

  if (check_unused_patches(session)) {
    return spn_err_reported(SPN_ERROR);
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_END,
    .sync_end = {
      .num_synced = sp_da_size(packages),
      .time = elapsed,
    }});

  return spn_result(SPN_OK);
}
