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

// We need a way to point to local copies of packages while developing; for a quick
// and dirty, just use SPN_PATCH_DIR as the package manifest repo if set.
//
// I don't intend for this to stick around forever, but works great now
static bool load_patched_package(spn_session_t* session, spn_loaded_pkg_t* loaded, spn_index_rel_t* release) {
  sp_str_t patches = sp_env_get(spn.env, sp_str_lit("SPN_PATCH_DIR"));
  if (sp_str_empty(patches)) return false;

  sp_str_t dir = sp_fs_join_path(session->mem, patches, release->id.name);
  sp_str_t manifest = sp_fs_join_path(session->mem, dir, release->paths.manifest);
  if (!sp_fs_exists(manifest)) return false;

  loaded->source = SPN_PKG_SOURCE_INDEX;
  loaded->paths.manifest = manifest;
  loaded->paths.script = sp_fs_join_path(session->mem, dir, release->paths.script);
  loaded->paths.source = dir;
  loaded->info = sp_alloc_type(session->mem, spn_pkg_info_t);
  spn_pkg_load(loaded->info, loaded->paths.manifest);

  // Packages whose source lives in a separate repo still need it checked out
  spn_pkg_info_t* info = loaded->info;
  spn_pkg_metadata_t* meta = sp_ht_getp(info->metadata, info->version);
  if (!sp_str_empty(info->url) && meta && !sp_str_empty(meta->commit)) {
    spn_git_checkout_t* checkout = SP_NULLPTR;
    spn_git_checkout_id_t id = {
      .url = info->url,
      .rev = meta->commit,
    };
    if (spn_git_cache_ensure_checkout(session->git, id, &checkout)) return false;
    loaded->paths.source = checkout->path;
  }

  return true;
}

spn_err_t load_index_package(spn_session_t* session, spn_resolved_pkg_t* resolved) {
  sp_str_ht_insert(session->packages, resolved->qualified, sp_zero_struct(spn_loaded_pkg_t));
  spn_loaded_pkg_t* loaded = sp_str_ht_get(session->packages, resolved->qualified);

  spn_index_rel_t* release = resolved->index.release;

  if (load_patched_package(session, loaded, release)) {
    return SPN_OK;
  }

  struct {
    spn_git_checkout_t* manifest;
    spn_git_checkout_t* source;
  } checkouts = sp_zero_initialize();

  sp_tm_timer_t timer = sp_tm_start_timer();

  // If the manifest is in a different repo than the source, check that out first
  if (!sp_str_empty(release->manifest.url)) {
    spn_git_checkout_id_t id = {
      .url = release->manifest.url,
      .rev = release->manifest.rev,
      .dir = release->manifest.dir
    };
    spn_try(spn_git_cache_ensure_checkout(session->git, id, &checkouts.manifest));
  }

  // If the package has source code, clone it
  if (!sp_str_empty(release->source.url)) {
    spn_git_checkout_id_t id = {
      .url = release->source.url,
      .rev = release->source.rev,
      .dir = release->source.dir
    };
    spn_try(spn_git_cache_ensure_checkout(session->git, id, &checkouts.source));
    loaded->paths.source = checkouts.source->path;
  }

  if (checkouts.manifest) {
    loaded->paths.manifest = sp_fs_join_path(session->mem, checkouts.manifest->path, release->paths.manifest);
    loaded->paths.script = sp_fs_join_path(session->mem, checkouts.manifest->path, release->paths.script);
  }
  else {
    loaded->paths.manifest = sp_fs_join_path(session->mem, checkouts.source->path, release->paths.manifest);
    loaded->paths.script = sp_fs_join_path(session->mem, checkouts.source->path, release->paths.script);
  }

  if (sp_fs_exists(loaded->paths.manifest)) {
    sp_assert_fmt(
      sp_fs_exists(loaded->paths.manifest),
      "manifest didn't exist; pkg = {}, path = {}, checkout = {}",
      SP_FMT_STR(spn_pkg_id_to_qualified_name(release->id)),
      SP_FMT_STR(loaded->paths.manifest),
      SP_FMT_PTR(checkouts.manifest)
    );
  }

  loaded->source = SPN_PKG_SOURCE_INDEX;
  loaded->info = sp_alloc_type(session->mem, spn_pkg_info_t);
  spn_pkg_load(loaded->info, loaded->paths.manifest);

  loaded->elapsed = sp_tm_read_timer(&timer);

  return SPN_OK;
}

spn_err_t load_file_package(spn_session_t* session, spn_resolved_pkg_t* pkg) {
  spn_loaded_pkg_t* loaded = spn_registry_load_file_pkg(&session->packages, pkg->qualified, pkg->file.path);
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
  spn_git_cache_init(session->git, spn.paths.source);

  // Load every package's manifest, checking out source code if needed
  sp_str_ht_for_kv(session->resolve, it) {
    spn_resolved_pkg_t* pkg = it.val;
    switch (pkg->source) {
      case SPN_PKG_SOURCE_ROOT: load_root_package(session); break;
      case SPN_PKG_SOURCE_INDEX: load_index_package(session, pkg); break;
      case SPN_PKG_SOURCE_FILE: load_file_package(session, pkg); break;
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
