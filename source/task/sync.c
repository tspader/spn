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

static bool match_toolchain(spn_toolchain_entry_t* toolchain, spn_triple_t host, spn_triple_t target) {
  spn_toolchain_info_t* info = &toolchain->info;
  if (toolchain->kind != SPN_TOOLCHAIN_INLINE) return false;

  struct {
    bool host;
    bool target;
  } match = SP_ZERO_INITIALIZE();

  // If a toolchain supports both the host and the target, use it. We store
  // the supported triples in a fixed size target and use a sentinel
  sp_carr_for(info->hosts, h) {
    if (!info->hosts[h].arch) break;

    if (spn_triple_match(info->hosts[h], host)) {
      match.host = true;
      break;
    }
  }
  if (!match.host) return false;

  sp_carr_for(info->targets, t) {
    if (!info->targets[t].arch) break;

    if (spn_triple_match(info->targets[t], target)) {
      match.target = true;
      break;
    }
  }
  if (!match.target) return false;

  return true;
}

static spn_toolchain_entry_t* find_toolchain(spn_pkg_info_t* pkg, spn_triple_t host, spn_triple_t target) {
  sp_str_om_for(pkg->toolchains, it) {
    spn_toolchain_entry_t* toolchain = sp_str_om_at(pkg->toolchains, it);

    if (match_toolchain(toolchain, host, target)) {
      return toolchain;
    }
  }

  return SP_NULLPTR;
}

static void log_toolchain_error(spn_pkg_info_t* pkg, spn_triple_t host, spn_triple_t target) {
  spn_log_error(
    "selected toolchain ({.fg cyan}, {}) does not support this host + target ({.fg yellow}, {.fg yellow})",
    SP_FMT_STR(pkg->name),
    SP_FMT_STR(spn_semver_to_str(pkg->version)),
    SP_FMT_STR(spn_triple_to_str(host)),
    SP_FMT_STR(spn_triple_to_str(target))
  );
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

  // The toolchain can be specified as either a package which exports toolchains
  // or as a block of TOML inline in the manifest.
  //
  // Inline toolchains are straightforward, but toolchains that are fetched from
  // a package can specify a URL which points to a tarball. Packages can also
  // export several toolchains.
  //
  // All we're doing here is finding the package manifest for the package that
  // defines the toolchain we want, grabbing the specific toolchain entry that
  // matches our host + target, and marking down any needed download.
  spn_triple_t host = spn_triple_host();
  spn_triple_t target = { session->profile.arch, session->profile.os, session->profile.abi };
  spn_toolchain_entry_t* entry = sp_str_ht_get(session->toolchains, session->profile.toolchain);
  sp_assert(entry);

  session->units.toolchain = sp_alloc_type(app->session.mem, spn_toolchain_unit_t);
  spn_toolchain_unit_t* toolchain = session->units.toolchain;
  toolchain->session = session;
  spn_lazy_log_init(&toolchain->logs.build, sp_fs_join_path(app->session.mem, spn.paths.log, sp_str_lit("toolchain.build.log")), SP_IO_WRITE_MODE_OVERWRITE);
  spn_lazy_log_init(&toolchain->logs.test,  sp_fs_join_path(app->session.mem, spn.paths.log, sp_str_lit("toolchain.test.log")),  SP_IO_WRITE_MODE_OVERWRITE);
  spn_lazy_log_init(&toolchain->logs.jsonl, sp_fs_join_path(app->session.mem, spn.paths.log, sp_str_lit("toolchain.jsonl")),     SP_IO_WRITE_MODE_OVERWRITE);

  if (entry->kind == SPN_TOOLCHAIN_INLINE) {
    if (!match_toolchain(entry, host, target)) {
      spn_log_error(
        "toolchain {.cyan} doesn't support host {.cyan} targeting {.cyan}",
        SP_FMT_STR(entry->name),
        SP_FMT_STR(spn_triple_to_str(host)),
        SP_FMT_STR(spn_triple_to_str(target))
      );
      return SPN_TASK_ERROR;
    }

    toolchain->kind = SPN_TOOLCHAIN_INLINE;
    toolchain->pkg = session->pkg;
  }
  else if (entry->kind == SPN_TOOLCHAIN_BUILTIN) {
    toolchain->kind = SPN_TOOLCHAIN_BUILTIN;
    toolchain->pkg = session->pkg;
  }
  else if (entry->kind == SPN_TOOLCHAIN_INDEX) {
    spn_loaded_pkg_t* pkg = sp_str_ht_get(session->packages, entry->request.package);
    sp_assert(pkg);

    entry = find_toolchain(pkg->info, host, target);
    if (!entry) {
      log_toolchain_error(pkg->info, host, target);
      return SPN_TASK_ERROR;
    }
    sp_assert(entry->kind == SPN_TOOLCHAIN_INDEX);

    toolchain->kind = SPN_TOOLCHAIN_INDEX;
    toolchain->pkg = pkg->info;
  }

  toolchain->info = entry->info;

  add_compilation_units(session, app->resolver);

  // If we need to download a toolchain, point it at the unit for the corresponding
  // package. This is separate from where we resolve the toolchain because we don't
  // want to special case the toolchain's package unit.
  //
  // By setting up the paths here, the toolchain can be downloaded to the package's
  // work dir and decompressed to the store dir with no special paths.
  switch (toolchain->kind) {
    case SPN_TOOLCHAIN_BUILTIN:
    case SPN_TOOLCHAIN_INLINE: {
      toolchain->compiler = entry->info.compiler;
      toolchain->linker = entry->info.linker;
      toolchain->archiver = entry->info.archiver;
      break;
    }
    case SPN_TOOLCHAIN_INDEX: {
      spn_toolchain_unit_t* unit = session->units.toolchain;
      spn_pkg_unit_id_t id = { sp_intern_get_or_insert(session->intern, unit->pkg->qualified) };
      spn_pkg_unit_t* pkg = sp_om_get(session->units.packages, id);

      sp_str_t store = pkg->paths.store;
      sp_str_t work = pkg->paths.work;

      // These are places in the cache
      unit->paths.store = store;
      unit->paths.work = work;
      unit->paths.stamp = sp_fs_join_path(app->session.mem, work, sp_str_lit("download.stamp"));
      unit->paths.logs.build = sp_fs_join_path(app->session.mem, work, sp_str_lit("build.log"));
      unit->paths.logs.jsonl = sp_fs_join_path(app->session.mem, work, sp_str_lit("build.jsonl"));
      spn_lazy_log_init(&unit->logs.build, unit->paths.logs.build, SP_IO_WRITE_MODE_OVERWRITE);
      spn_lazy_log_init(&unit->logs.jsonl, unit->paths.logs.jsonl, SP_IO_WRITE_MODE_OVERWRITE);

      // These are the paths used to refer to the toolchain during compilation
      unit->compiler.program = sp_fs_join_path(app->session.mem, store, unit->info.compiler.program);
      unit->compiler.args = unit->info.compiler.args;
      unit->linker.program = sp_fs_join_path(app->session.mem, store, unit->info.linker.program);
      unit->linker.args = unit->info.linker.args;
      unit->archiver.program = sp_fs_join_path(app->session.mem, store, unit->info.archiver.program);
      unit->archiver.args = unit->info.archiver.args;
      break;
    }
  }

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
