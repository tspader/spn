#include "sp.h"
#include "sp/str.h"
#include "cpu/cpu.h"
#include "ctx/ctx.h"
#include "ctx/types.h"
#include "dag/dag.h"
#include "error/error.h"
#include "spn/errors.h"
#include "event/event.h"
#include "core/types.h"
#include "git/cache.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
#include "op/op.h"
#include "paths/paths.h"
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
#include "toolchain/probe.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "unit/types.h"
#include "unit/unit.h"
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
    spn_event_buffer_push(spn.events, (spn_event_t) {
      .kind = SPN_EVENT_SYNC,
      .sync = {
        .name = name,
        .url = url,
      }});
  }

  spn_try(spn_toolchain_provision(store, toolchain, unit->artifact, &unit->root));

  spn_event_buffer_push(spn.events, (spn_event_t){
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
    spn_path_t root = spn_path_make(&spn.roots, unit->root);
    unit->cc.compiler = spn_toolchain_launcher_with_root(spn.mem, toolchain->compiler, root);
    unit->cc.cxx = spn_toolchain_launcher_with_root(spn.mem, toolchain->cxx, root);
    unit->cc.linker = spn_toolchain_launcher_with_root(spn.mem, toolchain->linker, root);
    unit->cc.archiver = spn_toolchain_launcher_with_root(spn.mem, toolchain->archiver, root);
  }

  switch (toolchain->source) {
    case SPN_TOOLCHAIN_SOURCE_LOCAL: {
      spn_try(spn_toolchain_probe(&unit->cc, spn_probe_split_path(spn.mem, spn_probe_env_path(spn.env)), &store->probes, spn.mem, &unit->identity));
      spn_probe_cache_flush(&store->probes);
      break;
    }
    case SPN_TOOLCHAIN_SOURCE_DISTRIBUTION: {
      unit->version = toolchain->version;
      break;
    }
  }

  return SPN_OK;
}

static spn_err_t materialize_tree(spn_session_t* session, sp_str_t name, spn_pkg_root_t tree, spn_path_t* root, bool* fetched) {
  switch (tree.kind) {
    case SPN_PKG_ROOT_LOCAL: {
      sp_str_t canonical = sp_fs_canonicalize_path(spn.mem, tree.local);
      if (sp_str_empty(canonical)) {
        return spn_err_emit(session->ctx, (spn_err_union_t) {
          .kind = SPN_ERR_NO_MANIFEST,
          .no_manifest = { .path = tree.local },
        });
      }
      *root = spn_path_make(&spn.roots, canonical);
      return SPN_OK;
    }
    case SPN_PKG_ROOT_GIT: {
      if (!spn_git_cache_is_checkout_cached(&session->ctx->caches.git, tree.git)) {
        spn_event_buffer_push(spn.events, (spn_event_t){
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
        spn_event_buffer_push(spn.events, (spn_event_t) {
          .kind = SPN_EVENT_SYNC_FAILED,
          .sync_failed = {
              .name = name,
              .url = tree.git.url,
              .error = error,
          }
        });
        return SPN_ERROR;
      }

      *root = spn_path_make(&spn.roots, checkout->path);
      *fetched |= checkout->fetched;
      return SPN_OK;
    }
    case SPN_PKG_ROOT_NONE: {
      *root = sp_zero_struct(spn_path_t);
      return SPN_OK;
    }
  }

  sp_unreachable_return(SPN_ERROR);
}

static sp_da(spn_path_t) resolve_paths(const spn_path_roots_t* roots, spn_gated_path_list_t entries, spn_loaded_pkg_t* loaded, spn_when_env_t* env) {
  sp_da(spn_path_t) resolved = sp_da_new(spn.mem, spn_path_t);
  sp_da_for(entries, it) {
    if (!spn_when_eval(&entries[it].when, env)) {
      continue;
    }
    spn_path_t path = spn_tree_path(spn.mem, roots, loaded->roots, entries[it].tree, entries[it].path);
    sp_da_push(resolved, spn_path_canonicalize(spn.mem, roots, path));
  }
  return resolved;
}

static spn_err_t resolve_configure_source(spn_ctx_t* ctx, sp_str_t name, spn_gated_path_list_t declared, spn_loaded_pkg_t* loaded, spn_when_env_t* env, sp_da(spn_path_t)* source) {
  sp_da(spn_path_t) resolved = sp_da_new(spn.mem, spn_path_t);
  sp_da_for(declared, it) {
    if (!spn_when_eval(&declared[it].when, env)) {
      continue;
    }
    spn_path_t path = spn_tree_path(spn.mem, &ctx->roots, loaded->roots, declared[it].tree, declared[it].path);
    spn_tree_rel_t rel = spn_tree_rel(loaded->roots, path);
    if (rel.tree == SPN_TREE_NONE || !sp_fs_is_glob(rel.sub)) {
      if (!sp_fs_is_target_file(spn_path_str(&ctx->roots, spn.mem, path))) {
        return spn_err_emit(ctx, (spn_err_union_t) {
          .kind = SPN_ERR_CONFIGURE_SOURCE_MISSING,
          .configure_source = {
            .name = name,
            .source = declared[it].path,
          }});
      }
      sp_da_push(resolved, spn_path_canonicalize(spn.mem, &ctx->roots, path));
      continue;
    }

    sp_da(spn_path_t) matches = sp_da_new(spn.mem, spn_path_t);
    if (spn_dag_glob(spn.mem, &ctx->roots, path, SP_NULLPTR, &matches) || sp_da_empty(matches)) {
      return spn_err_emit(ctx, (spn_err_union_t) {
        .kind = SPN_ERR_CONFIGURE_SOURCE_GLOB,
        .configure_source = {
          .name = name,
          .source = declared[it].path,
        }});
    }
    sp_da_for(matches, jt) {
      sp_da_push(resolved, spn_path_canonicalize(spn.mem, &ctx->roots, matches[jt]));
    }
  }
  *source = resolved;
  return SPN_OK;
}

static sp_da(spn_path_t) detect_configure_source(const spn_path_roots_t* roots, spn_loaded_pkg_t* loaded, sp_str_t script) {
  sp_da(spn_path_t) source = sp_da_new(spn.mem, spn_path_t);
  sp_str_t candidates [] = { sp_str_lit("configure.c"), script };
  sp_carr_for(candidates, it) {
    spn_path_t path = spn_tree_path(spn.mem, roots, loaded->roots, SPN_TREE_MANIFEST, candidates[it]);
    if (sp_fs_is_target_file(spn_path_str(roots, spn.mem, path))) {
      sp_da_push(source, spn_path_canonicalize(spn.mem, roots, path));
      break;
    }
  }
  return source;
}

static spn_err_t load_manifest(spn_session_t* session, sp_str_t name, sp_str_t path, spn_pkg_info_t** info) {
  spn_pkg_info_t* parsed = sp_alloc_type(spn.mem, spn_pkg_info_t);
  spn_codegen_issues_t issues = sp_zero;
  spn_err_t loaded = spn_pkg_load(spn.mem, session->ctx->intern, path, SPN_MANIFEST_DEP, parsed, &issues);
  if (loaded == SPN_ERR_NO_MANIFEST) {
    return spn_err_emit(session->ctx, (spn_err_union_t) {
      .kind = SPN_ERR_NO_MANIFEST,
      .no_manifest = { .path = path },
    });
  }
  if (loaded) {
    return spn_err_emit(session->ctx, (spn_err_union_t) {
      .kind = SPN_ERR_MANIFEST_ISSUES,
      .manifest = { .name = name, .path = path, .issues = spn_codegen_issues_to_err(spn.mem, issues) },
    });
  }

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
        spn_event_buffer_push(spn.events, (spn_event_t) {
          .kind = SPN_EVENT_SYNC_PATCH,
          .sync = {
            .name = qualified,
            .url = pkg->origin.source.git.url,
          }});
      }
      return SPN_OK;
    }
    case SPN_PKG_PATCH_STAMP_NOT_GIT: {
      return spn_err_emit(session->ctx, (spn_err_union_t) {
        .kind = SPN_ERR_PATCH_NOT_GIT,
        .patch = { .name = qualified },
      });
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

  const spn_path_roots_t* roots = &spn.roots;
  loaded->info = pkg->origin.info;
  if (!loaded->info) {
    spn_path_t manifest = spn_path_join(spn.mem, loaded->roots.recipe, pkg->origin.paths.manifest);
    spn_try(load_manifest(session, qualified, spn_path_str(roots, spn.mem, manifest), &loaded->info));
  }

  if (pkg->origin.source.kind == SPN_PKG_ROOT_NONE) {
    loaded->roots.source = loaded->roots.recipe;
  } else {
    spn_try(materialize_tree(session, qualified, pkg->origin.source, &loaded->roots.source, &fetched));
  }

  spn_when_env_t facts = sp_zero;
  spn_when_env_from_profile(spn.mem, &session->profile, &facts);

  loaded->build = loaded->info->build;
  loaded->build.source = resolve_paths(roots, loaded->info->build.gated.source, loaded, &facts);
  loaded->build.include = resolve_paths(roots, loaded->info->build.gated.include, loaded, &facts);

  loaded->configure = loaded->info->configure;
  loaded->configure.include = resolve_paths(roots, loaded->info->configure.gated.include, loaded, &facts);

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
    loaded->configure.source = detect_configure_source(roots, loaded, pkg->origin.paths.script);
  } else {
    spn_try(resolve_configure_source(session->ctx, qualified, loaded->info->configure.gated.source, loaded, &facts, &loaded->configure.source));
  }

  if (sp_da_empty(loaded->build.source)) {
    spn_path_t candidate = spn_tree_path(spn.mem, roots, loaded->roots, SPN_TREE_MANIFEST, sp_str_lit("build.c"));
    spn_path_t script = spn_tree_path(spn.mem, roots, loaded->roots, SPN_TREE_MANIFEST, pkg->origin.paths.script);
    if (sp_fs_is_target_file(spn_path_str(roots, spn.mem, candidate))) {
      sp_da_push(loaded->build.source, spn_path_canonicalize(spn.mem, roots, candidate));
    }
    else if (package_has_build_deps(pkg) && sp_fs_is_target_file(spn_path_str(roots, spn.mem, script))) {
      sp_da_push(loaded->build.source, spn_path_canonicalize(spn.mem, roots, script));
    }
  }

  loaded->elapsed = sp_tm_read_timer(&timer);

  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_SYNC_PACKAGE, .pkg = loaded->info->name,
    .sync_pkg = {
      .name = qualified,
      .url = sync_url(pkg->origin.recipe, pkg->origin.source),
      .source_path = spn_path_str(roots, spn.mem, loaded->roots.source),
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

    spn_err_t kind = spn_err_emit(session->ctx, (spn_err_union_t) {
      .kind = SPN_ERR_PATCH_UNUSED,
      .patch = { .name = qualified },
    });
    if (!err) {
      err = kind;
    }
  }
  return err;
}

spn_err_t sync_packages(spn_op_t* op, bool* reresolve) {
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

  spn_event_buffer_push(spn.events, (spn_event_t) {
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
    .workers = sp_min(spn_cpu_count(), num_jobs),
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
    return SPN_ERR_CANCELLED;
  }

  sp_da_for(packages, it) {
    spn_try(packages[it]->err);
  }
  sp_da_for(toolchains, it) {
    spn_try(toolchains[it]->err);
  }

  sp_da_for(packages, it) {
    pkg_job_t* job = packages[it];
    job->pkg->name = job->loaded.info->name;
    job->pkg->options = job->loaded.info->options;
    sp_ht_insert(session->packages, job->pkg->id, job->loaded);
  }

  spn_try(spn_session_apply_options(session, reresolve));

  if (*reresolve) {
    return SPN_OK;
  }

  spn_session_export_toolchain_env(session);
  spn_try(spn_session_validate_flags(session));
  spn_try(check_unused_patches(session));

  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_SYNC_END,
    .sync_end = {
      .num_synced = sp_da_size(packages),
      .time = elapsed,
    }});

  return SPN_OK;
}
