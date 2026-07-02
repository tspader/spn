#include "app/app.h"
#include "app/types.h"
#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "ctx/ctx.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/event.h"
#include "forward/types.h"
#include "git/cache.h"
#include "index/types.h"
#include "intern/intern.h"
#include "log/log.h"
#include "log/lazy/lazy.h"
#include "pkg/id.h"
#include "pkg/load.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "semver/convert.h"
#include "session/registry/registry.h"
#include "session/session.h"
#include "session/types.h"
#include "spn.h"
#include "sp/macro.h"
#include "task/task.h"
#include "task/types.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "triple/triple.h"
#include "unit/types.h"
#include <unistd.h>

SP_PRIVATE spn_toolchain_unit_t* setup_toolchain_unit(spn_session_t* session, spn_toolchain_store_t* store, spn_toolchain_t* toolchain) {
  spn_toolchain_unit_t* unit = sp_alloc_type(session->mem, spn_toolchain_unit_t);
  unit->session = session;
  unit->toolchain = toolchain;

  sp_str_t name = toolchain->name;
  spn_lazy_log_init(&unit->logs.build, sp_fs_join_path(session->mem, spn.paths.log, sp_fmt(session->mem, "toolchain.{}.build.log", sp_fmt_str(name)).value));
  spn_lazy_log_init(&unit->logs.test,  sp_fs_join_path(session->mem, spn.paths.log, sp_fmt(session->mem, "toolchain.{}.test.log",  sp_fmt_str(name)).value));
  spn_lazy_log_init(&unit->logs.jsonl, sp_fs_join_path(session->mem, spn.paths.log, sp_fmt(session->mem, "toolchain.{}.jsonl",     sp_fmt_str(name)).value));

  if (spn_toolchain_provision(store, toolchain, &unit->root)) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_FAILED,
      .sync_failed = {
        .name = name,
        .url = sp_opt_is_null(toolchain->artifact) ? sp_str_lit("") : sp_opt_get(toolchain->artifact).url,
        .error = sp_str_lit("failed to provision toolchain"),
      }
    });
    return SP_NULLPTR;
  }

  if (sp_str_empty(unit->root)) {
    unit->compiler = toolchain->compiler;
    unit->linker = toolchain->linker;
    unit->archiver = toolchain->archiver;
  }
  else {
    unit->compiler = spn_toolchain_launcher_with_root(session->mem, toolchain->compiler, unit->root);
    unit->linker   = spn_toolchain_launcher_with_root(session->mem, toolchain->linker, unit->root);
    unit->archiver = spn_toolchain_launcher_with_root(session->mem, toolchain->archiver, unit->root);
  }

  return unit;
}

static spn_pkg_tree_t tree_local(sp_str_t path) {
  return (spn_pkg_tree_t) { .kind = SPN_PKG_TREE_LOCAL, .local = path };
}

static spn_pkg_tree_t tree_git(spn_index_rel_source_t source) {
  return (spn_pkg_tree_t) {
    .kind = SPN_PKG_TREE_GIT,
    .git = { .url = source.url, .rev = source.rev, .dir = source.dir },
  };
}

// Place a tree on disk and hand back its root: a local path is already there;
// a git tree is checked out through the cache.
static spn_err_t materialize_tree(spn_session_t* session, spn_pkg_tree_t tree, sp_str_t* out) {
  switch (tree.kind) {
    case SPN_PKG_TREE_LOCAL: {
      *out = tree.local;
      return SPN_OK;
    }
    case SPN_PKG_TREE_GIT: {
      spn_git_checkout_t* checkout = SP_NULLPTR;
      spn_try(spn_git_cache_ensure_checkout(session->git, tree.git, &checkout));
      *out = checkout->path;
      return SPN_OK;
    }
    case SPN_PKG_TREE_NONE: {
      *out = sp_str_lit("");
      return SPN_OK;
    }
  }

  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t fail_sync(sp_str_t name, spn_pkg_tree_t tree) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_FAILED,
    .sync_failed = {
      .name = name,
      .url = tree.kind == SPN_PKG_TREE_GIT ? tree.git.url : tree.local,
      .error = sp_str_lit("failed to fetch repository"),
    }
  });
  return SPN_ERROR;
}

// The one place a spn_loaded_pkg_t is constructed. Materialize the recipe
// tree, parse the manifest if the caller doesn't already have its info, then
// materialize the source tree. When `derive_source` is set the source tree is
// read from the manifest (the local-recipe case: root, file deps, patched
// index packages); otherwise the caller supplies it (the published-release
// case). A NONE source means the code lives alongside the recipe.
static spn_err_t load_package(
  spn_session_t* session,
  spn_loaded_pkg_t* loaded,
  sp_str_t name,
  spn_pkg_info_t* info,
  spn_pkg_tree_t recipe_tree,
  spn_index_rel_paths_t rel,
  bool derive_source,
  spn_pkg_tree_t source_tree
) {
  sp_tm_timer_t timer = sp_tm_start_timer();

  if (materialize_tree(session, recipe_tree, &loaded->roots.recipe)) {
    return fail_sync(name, recipe_tree);
  }

  loaded->paths.manifest = sp_fs_join_path(session->mem, loaded->roots.recipe, rel.manifest);
  loaded->paths.script = sp_fs_join_path(session->mem, loaded->roots.recipe, rel.script);

  if (!info) {
    info = sp_alloc_type(session->mem, spn_pkg_info_t);
    spn_codegen_ctx_t ctx = sp_zero;
    spn_codegen_ctx_init(&ctx, spn.mem, session->intern);
    if (spn_codegen_load_pkg(&ctx, loaded->paths.manifest, info)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_MANIFEST,
        .manifest_err = {
          .name = name,
          .path = loaded->paths.manifest,
          .error = spn_codegen_issues_message(session->mem, ctx.issues),
        }
      });
      return SPN_ERROR;
    }
  }

  loaded->info = info;
  loaded->paths.configure = sp_fs_join_path(session->mem, loaded->roots.recipe, info->configure);
  loaded->paths.build = sp_fs_join_path(session->mem, loaded->roots.recipe, info->build);

  if (derive_source) {
    source_tree = spn_pkg_manifest_source_tree(info);
  }

  if (source_tree.kind == SPN_PKG_TREE_NONE) {
    loaded->roots.source = loaded->roots.recipe;
  }
  else if (materialize_tree(session, source_tree, &loaded->roots.source)) {
    return fail_sync(name, source_tree);
  }

  loaded->elapsed = sp_tm_read_timer(&timer);
  return SPN_OK;
}

// While developing a recipe you don't want to publish on every edit, so
// SPN_PATCH_DIR/<name> stands in for the manifest repo. It's a plain tree
// substitution: the recipe comes from disk, the source is derived from that
// local manifest just like any other local recipe.
static sp_str_t patch_dir(spn_session_t* session, spn_index_rel_t* release) {
  sp_str_t patches = sp_env_get(spn.env, sp_str_lit("SPN_PATCH_DIR"));
  if (sp_str_empty(patches)) return sp_str_lit("");

  sp_str_t dir = sp_fs_join_path(session->mem, patches, release->id.name);
  sp_str_t manifest = sp_fs_join_path(session->mem, dir, release->paths.manifest);
  if (!sp_fs_exists(manifest)) return sp_str_lit("");

  return dir;
}

static spn_index_rel_paths_t local_rel_paths(sp_str_t manifest) {
  sp_str_t name = sp_str_strip_left(manifest, sp_fs_parent_path(manifest));
  return (spn_index_rel_paths_t) {
    .manifest = sp_str_strip_left(name, sp_str_lit("/")),
    .script = sp_str_lit("spn.c"),
  };
}

static spn_err_t load_index_package(spn_session_t* session, spn_resolved_pkg_t* resolved, spn_loaded_pkg_t* loaded) {
  spn_index_rel_t* release = resolved->index.release;

  sp_str_t patch = patch_dir(session, release);
  if (!sp_str_empty(patch)) {
    return load_package(session, loaded, resolved->qualified, SP_NULLPTR, tree_local(patch), release->paths, true, sp_zero_s(spn_pkg_tree_t));
  }

  // The recipe lives in the manifest repo if there is one, otherwise the
  // source repo carries it. The source is whatever the release pinned.
  spn_pkg_tree_t recipe_tree = !sp_str_empty(release->manifest.url)
    ? tree_git(release->manifest)
    : tree_git(release->source);
  spn_pkg_tree_t source_tree = !sp_str_empty(release->source.url)
    ? tree_git(release->source)
    : (spn_pkg_tree_t) { .kind = SPN_PKG_TREE_NONE };

  return load_package(session, loaded, resolved->qualified, SP_NULLPTR, recipe_tree, release->paths, false, source_tree);
}

static spn_err_t load_file_package(spn_session_t* session, spn_resolved_pkg_t* pkg, spn_loaded_pkg_t* loaded) {
  spn_registry_pkg_t* entry = sp_str_ht_get(session->registry, pkg->qualified);
  sp_assert(entry);

  sp_str_t dir = sp_fs_parent_path(entry->manifest);
  return load_package(session, loaded, pkg->qualified, entry->info, tree_local(dir), local_rel_paths(entry->manifest), true, sp_zero_s(spn_pkg_tree_t));
}

static spn_err_t load_root_package(spn_session_t* session, spn_loaded_pkg_t* loaded) {
  return load_package(session, loaded, session->pkg->qualified, session->pkg, tree_local(spn.paths.project), local_rel_paths(spn.paths.manifest), false, sp_zero_s(spn_pkg_tree_t));
}

void add_compilation_units(spn_session_t* session, spn_resolver_t* resolver) {
  sp_str_ht_for_kv(session->packages, it) {
    spn_session_add_pkg(session, it.val);
  }

  sp_str_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* pkg = it.val;
    spn_pkg_unit_t* unit = spn_session_find_pkg_by_qualified(session, pkg->qualified);

    sp_da(spn_pkg_unit_t*) deps = sp_da_new(session->mem, spn_pkg_unit_t*);
    sp_da_for(pkg->deps, j) {
      spn_pkg_unit_t* dep = spn_session_find_pkg_by_qualified(session, pkg->deps[j].qualified);
      sp_da_push(deps, dep);
    }

    sp_om_insert(session->units.graph, unit->id, deps);
  }
}

spn_task_result_t spn_task_sync_init(spn_app_t* app) {
  spn_session_t* session = &app->session;

  session->git = sp_alloc_type(app->session.mem, spn_git_cache_t);
  spn_git_cache_init(session->git, session->mem, session->intern, spn.paths.source);

  // Load every package's manifest, checking out source code if needed
  sp_str_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* pkg = it.val;

    spn_loaded_pkg_t loaded = sp_zero_struct(spn_loaded_pkg_t);
    loaded.source = pkg->source;

    switch (pkg->source) {
      case SPN_PKG_SOURCE_ROOT: spn_try_as(load_root_package(session, &loaded), SPN_TASK_ERROR); break;
      case SPN_PKG_SOURCE_INDEX: spn_try_as(load_index_package(session, pkg, &loaded), SPN_TASK_ERROR); break;
      case SPN_PKG_SOURCE_FILE: spn_try_as(load_file_package(session, pkg, &loaded), SPN_TASK_ERROR); break;
    }

    sp_str_ht_insert(session->packages, pkg->qualified, loaded);
  }

  spn_toolchain_store_t store = {
    .mem = session->mem,
    .dir = spn.paths.toolchain,
    .mirror = sp_env_get(spn.env, sp_str_lit("SPN_MIRROR")),
    .fetch = spn_fetch_curl,
  };

  sp_da_for(session->selection.required, it) {
    spn_toolchain_t* toolchain = session->selection.required[it];
    spn_toolchain_unit_t* unit = setup_toolchain_unit(session, &store, toolchain);
    if (!unit) return SPN_TASK_ERROR;

    sp_da_push(session->units.toolchains, unit);
    if (toolchain == session->selection.build)  session->units.toolchain = unit;
    if (toolchain == session->selection.script) session->units.script = unit;
  }

  add_compilation_units(session, app->resolver);

  sp_env_t* env = &session->env;
  sp_env_init(app->session.mem, env);
  sp_env_insert(env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->compiler));
  sp_env_insert(env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->archiver));
  sp_env_insert(env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->linker));

  return SPN_TASK_CONTINUE;
}

spn_task_result_t spn_task_sync_update(spn_app_t* app) {
  return SPN_TASK_DONE;
}
