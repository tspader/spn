#include "app/app.h"
#include "app/types.h"
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

SP_PRIVATE spn_toolchain_unit_t* setup_toolchain_unit(spn_session_t* session, spn_toolchain_entry_t* entry) {
  spn_toolchain_unit_t* unit = sp_alloc_type(session->mem, spn_toolchain_unit_t);
  unit->session = session;
  unit->kind = entry->kind;
  unit->info = entry->info;
  unit->pkg = session->pkg;

  sp_str_t name = entry->name;
  spn_lazy_log_init(&unit->logs.build, sp_fs_join_path(session->mem, spn.paths.log, sp_format("toolchain.{}.build.log", SP_FMT_STR(name))), SP_IO_WRITE_MODE_OVERWRITE);
  spn_lazy_log_init(&unit->logs.test,  sp_fs_join_path(session->mem, spn.paths.log, sp_format("toolchain.{}.test.log",  SP_FMT_STR(name))), SP_IO_WRITE_MODE_OVERWRITE);
  spn_lazy_log_init(&unit->logs.jsonl, sp_fs_join_path(session->mem, spn.paths.log, sp_format("toolchain.{}.jsonl",     SP_FMT_STR(name))), SP_IO_WRITE_MODE_OVERWRITE);

  if (sp_str_empty(entry->info.url)) {
    unit->compiler = entry->info.compiler;
    unit->linker = entry->info.linker;
    unit->archiver = entry->info.archiver;
    return unit;
  }

  sp_str_t key = sp_format("{}-{}-{}", SP_FMT_STR(name), SP_FMT_STR(entry->info.version), SP_FMT_STR(entry->info.sha));
  sp_str_t store = sp_fs_join_path(session->mem, spn.paths.toolchain, key);
  sp_fs_create_dir(store);

  unit->url = entry->info.url;
  unit->paths.store = store;
  unit->paths.work = store;
  unit->paths.stamp = sp_fs_join_path(session->mem, store, sp_str_lit("download.stamp"));

  unit->compiler = spn_toolchain_launcher_with_root(entry->info.compiler, store);
  unit->linker   = spn_toolchain_launcher_with_root(entry->info.linker, store);
  unit->archiver = spn_toolchain_launcher_with_root(entry->info.archiver, store);

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
static sp_str_t materialize_tree(spn_session_t* session, spn_pkg_tree_t tree) {
  switch (tree.kind) {
    case SPN_PKG_TREE_LOCAL: return tree.local;
    case SPN_PKG_TREE_GIT: {
      spn_git_checkout_t* checkout = SP_NULLPTR;
      if (spn_git_cache_ensure_checkout(session->git, tree.git, &checkout)) return sp_str_lit("");
      return checkout->path;
    }
    case SPN_PKG_TREE_NONE: return sp_str_lit("");
  }

  sp_unreachable_return(sp_str_lit(""));
}

// Materialize a recipe (manifest + build script), parse it, then materialize
// its source. When `derive_source` is set the source tree is read from the
// freshly parsed manifest (the local-recipe case: file deps, patched index
// packages); otherwise the caller supplies it (the published-release case).
// An empty/NONE source means the code lives alongside the manifest.
static void load_from_tree(
  spn_session_t* session,
  spn_loaded_pkg_t* loaded,
  spn_pkg_tree_t manifest_tree,
  spn_index_rel_paths_t paths,
  bool derive_source,
  spn_pkg_tree_t source_tree
) {
  sp_tm_timer_t timer = sp_tm_start_timer();

  sp_str_t root = materialize_tree(session, manifest_tree);
  loaded->paths.manifest = sp_fs_join_path(session->mem, root, paths.manifest);
  loaded->paths.script = sp_fs_join_path(session->mem, root, paths.script);

  loaded->info = sp_alloc_type(session->mem, spn_pkg_info_t);
  spn_pkg_load(loaded->info, loaded->paths.manifest);

  if (derive_source) {
    source_tree = spn_pkg_manifest_source_tree(loaded->info);
  }
  loaded->paths.source = source_tree.kind == SPN_PKG_TREE_NONE
    ? root
    : materialize_tree(session, source_tree);

  loaded->elapsed = sp_tm_read_timer(&timer);
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

spn_err_t load_index_package(spn_session_t* session, spn_resolved_pkg_t* resolved) {
  sp_str_ht_insert(session->packages, resolved->qualified, sp_zero_struct(spn_loaded_pkg_t));
  spn_loaded_pkg_t* loaded = sp_str_ht_get(session->packages, resolved->qualified);
  loaded->source = SPN_PKG_SOURCE_INDEX;

  spn_index_rel_t* release = resolved->index.release;

  sp_str_t patch = patch_dir(session, release);
  if (!sp_str_empty(patch)) {
    load_from_tree(session, loaded, tree_local(patch), release->paths, true, (spn_pkg_tree_t) { 0 });
    return SPN_OK;
  }

  // The recipe lives in the manifest repo if there is one, otherwise the
  // source repo carries it. The source is whatever the release pinned.
  spn_pkg_tree_t manifest_tree = !sp_str_empty(release->manifest.url)
    ? tree_git(release->manifest)
    : tree_git(release->source);
  spn_pkg_tree_t source_tree = !sp_str_empty(release->source.url)
    ? tree_git(release->source)
    : (spn_pkg_tree_t) { .kind = SPN_PKG_TREE_NONE };

  load_from_tree(session, loaded, manifest_tree, release->paths, false, source_tree);
  return SPN_OK;
}

spn_err_t load_file_package(spn_session_t* session, spn_resolved_pkg_t* pkg) {
  spn_loaded_pkg_t* loaded = spn_registry_load_file_pkg(&session->packages, session->mem, session->intern, pkg->qualified, pkg->file.path);
  if (!loaded) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_FAILED,
      .sync_failed = {
        .name = pkg->qualified,
        .url = pkg->file.path,
        .error = sp_str_lit("manifest not found"),
      }
    });
    return SPN_ERROR;
  }

  // A local recipe can pin its source to a separate repo just like a published
  // one. The registry can't check it out (it runs before the git cache exists),
  // so do it here where the source tree is materialized like every other.
  spn_pkg_tree_t source = spn_pkg_manifest_source_tree(loaded->info);
  if (source.kind != SPN_PKG_TREE_NONE) {
    loaded->paths.source = materialize_tree(session, source);
  }

  return SPN_OK;
}

spn_err_t load_root_package(spn_session_t* session) {
  spn_loaded_pkg_t* existing = sp_str_ht_get(session->packages, session->pkg->qualified);
  if (existing) return SPN_OK;

  sp_str_ht_insert(session->packages, session->pkg->qualified, sp_zero_struct(spn_loaded_pkg_t));
  spn_loaded_pkg_t* loaded = sp_str_ht_get(session->packages, session->pkg->qualified);

  loaded->info = session->pkg;
  loaded->source = SPN_PKG_SOURCE_ROOT;
  loaded->paths.manifest = spn.paths.manifest;
  loaded->paths.script = sp_fs_join_path(session->mem, spn.paths.project, sp_str_lit("spn.c"));
  loaded->paths.build = sp_fs_join_path(spn.mem, spn.paths.project, session->pkg->build);
  loaded->paths.configure = sp_fs_join_path(spn.mem, spn.paths.project, session->pkg->configure);
  loaded->paths.source = spn.paths.project;

  return SPN_OK;
}

void add_compilation_units(spn_session_t* session, spn_resolver_t* resolver) {
  sp_str_ht_for_kv(session->packages, it) {
    spn_session_add_pkg(session, it.val);
  }

  sp_str_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* pkg = it.val;
    spn_pkg_unit_t* unit = spn_session_find_pkg_by_qualified(session, pkg->qualified);

    sp_da(spn_pkg_unit_t*) deps = sp_zero;
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
    switch (pkg->source) {
      case SPN_PKG_SOURCE_ROOT: spn_try_as(load_root_package(session), SPN_TASK_ERROR); break;
      case SPN_PKG_SOURCE_INDEX: spn_try_as(load_index_package(session, pkg), SPN_TASK_ERROR); break;
      case SPN_PKG_SOURCE_FILE: spn_try_as(load_file_package(session, pkg), SPN_TASK_ERROR); break;
    }
  }

  spn_toolchain_entry_t* entry = sp_str_ht_get(session->toolchains, session->profile.toolchain);
  sp_assert(entry);
  session->units.toolchain = setup_toolchain_unit(session, entry);

  spn_toolchain_entry_t* zig = sp_str_ht_get(session->toolchains, sp_str_lit("zig"));
  sp_assert(zig);
  session->units.zig = setup_toolchain_unit(session, zig);

  add_compilation_units(session, app->resolver);

  sp_env_t* env = &session->env;
  sp_env_init(app->session.mem, env);
  sp_env_insert(env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(session->units.toolchain->compiler));
  sp_env_insert(env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(session->units.toolchain->archiver));
  sp_env_insert(env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(session->units.toolchain->linker));

  return SPN_TASK_CONTINUE;
}

spn_task_result_t spn_task_sync_update(spn_app_t* app) {
  return SPN_TASK_DONE;
}
