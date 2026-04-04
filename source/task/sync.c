#include "app/app.h"
#include "app/types.h"
#include "ctx/ctx.h"
#include "ctx/types.h"
#include "err.h"
#include "event/event.h"
#include "git/cache.h"
#include "index/types.h"
#include "log/log.h"
#include "pkg/id.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "semver/convert.h"
#include "session/session.h"
#include "spn.h"
#include "task/task.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "triple/triple.h"
#include <unistd.h>

typedef struct {
  spn_resolved_pkg_t* resolved;
  spn_pkg_t* pkg;
  sp_str_t manifest;
  sp_str_t script;
  sp_str_t source;
  u64 elapsed;
} checkout_t;

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

static spn_toolchain_entry_t* find_toolchain(spn_pkg_t* pkg, spn_triple_t host, spn_triple_t target) {
  sp_om_for(pkg->toolchains, it) {
    spn_toolchain_entry_t* toolchain = sp_om_at(pkg->toolchains, it);

    if (match_toolchain(toolchain, host, target)) {
      return toolchain;
    }
  }

  return SP_NULLPTR;
}

static void log_toolchain_error(spn_pkg_t* pkg, spn_triple_t host, spn_triple_t target) {
  spn_log_error(
    "selected toolchain ({:fg cyan}, {}) does not support this host + target ({:fg yellow}, {:fg yellow})",
    SP_FMT_STR(pkg->name),
    SP_FMT_STR(spn_semver_to_str(pkg->version)),
    SP_FMT_STR(spn_triple_to_str(host)),
    SP_FMT_STR(spn_triple_to_str(target))
  );
}

static spn_err_t sync_package(spn_session_t* session, spn_resolved_pkg_t* resolved, spn_git_cache_t* cache, checkout_t* checkout) {
  spn_err_t err = SPN_OK;
  spn_index_rel_t* release = resolved->release;

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
    spn_try(spn_git_cache_ensure_checkout(cache, id, &checkouts.manifest));
  }

  // If the package has source code, clone it
  if (!sp_str_empty(release->source.url)) {
    spn_git_checkout_id_t id = {
      .url = release->source.url,
      .rev = release->source.rev,
      .dir = release->source.dir
    };
    spn_try(spn_git_cache_ensure_checkout(cache, id, &checkouts.source));
    checkout->source = checkouts.source->path;
  }

  if (checkouts.manifest) {
    checkout->manifest = sp_fs_join_path(checkouts.manifest->path, release->paths.manifest);
    checkout->script = sp_fs_join_path(checkouts.manifest->path, release->paths.script);
  }
  else {
    checkout->manifest = sp_fs_join_path(checkouts.source->path, release->paths.manifest);
    checkout->script = sp_fs_join_path(checkouts.source->path, release->paths.script);
  }

  if (sp_fs_exists(checkout->manifest)) {
    sp_assert_fmt(
      sp_fs_exists(checkout->manifest),
      "manifest didn't exist; pkg = {}, path = {}, checkout = {}",
      SP_FMT_STR(spn_pkg_id_to_qualified_name(release->id)),
      SP_FMT_STR(checkout->manifest),
      SP_FMT_PTR(checkouts.manifest)
    );
  }

  checkout->elapsed = sp_tm_read_timer(&timer);

  return SPN_OK;
}

static spn_err_t load_package(spn_resolved_pkg_t* resolved, checkout_t* checkout) {
  spn_index_rel_t* release = resolved->release;

  spn_pkg_t* pkg = sp_alloc_type(spn_pkg_t);
  spn_pkg_init(pkg, release->id.name);
  spn_pkg_from_manifest(pkg, checkout->manifest);
  spn_pkg_add_version_ex(pkg, resolved->version, release->source.rev);

  pkg->kind = SPN_PACKAGE_KIND_INDEX;
  pkg->paths.root = checkout->source;
  pkg->paths.script = checkout->script;
  pkg->paths.manifest = checkout->manifest;
  pkg->paths.cache.source = checkout->source;
  pkg->paths.cache.work = sp_fs_join_path(spn.paths.build, pkg->qualified);
  pkg->paths.cache.store = sp_fs_join_path(spn.paths.store, pkg->qualified);

  resolved->pkg = pkg;
  checkout->pkg = pkg;

  return SPN_OK;
}

spn_task_result_t spn_task_sync_init(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_resolver_t* resolver = app->resolver;
  spn_pkg_unit_t* root = spn_session_find_root(session);

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_START,
    .sync_start.num_packages = sp_str_ht_size(app->package.deps)
  });

  struct {
    sp_tm_timer_t toolchain;
    sp_tm_timer_t git;
    sp_tm_timer_t pkg;
  } timers = sp_zero_initialize();

  timers.git = sp_tm_start_timer();

  // Load index dependencies via git cache. After this, we expect every dependency to
  // be checked out in the cache, and manifests loaded.
  spn_git_cache_t cache = SP_ZERO_INITIALIZE();
  spn_git_cache_init(&cache, spn.paths.source);

  sp_str_ht(checkout_t) checkouts = SP_ZERO_INITIALIZE();

  sp_str_ht_for_kv(resolver->resolved, it) {
    spn_resolved_pkg_t* resolved = it.val;
    if (resolved->kind != SPN_PACKAGE_KIND_INDEX) {
      continue;
    }

    checkout_t checkout = {
      .resolved = resolved,
    };
    spn_try_as(sync_package(session, resolved, &cache, &checkout), SPN_TASK_ERROR);
    spn_try_as(load_package(resolved, &checkout), SPN_TASK_ERROR);
    sp_str_ht_insert(checkouts, *it.key, checkout);
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

  if (entry->kind == SPN_TOOLCHAIN_INLINE) {
    if (!match_toolchain(entry, host, target)) {
      spn_log_error(
        "toolchain {:fg brightcyan} doesn't support host {:fg brightcyan} targeting {:fg brightcyan}",
        SP_FMT_STR(entry->name),
        SP_FMT_STR(spn_triple_to_str(host)),
        SP_FMT_STR(spn_triple_to_str(target))
      );
      return SPN_TASK_ERROR;
    }

    session->toolchain.source = SPN_TOOLCHAIN_INLINE;
    session->toolchain.info = entry->info;
    session->toolchain.compiler = session->toolchain.info.compiler;
    session->toolchain.linker = session->toolchain.info.linker;
    session->toolchain.archiver = session->toolchain.info.archiver;
    session->toolchain.pkg = session->pkg;
  }
  else if (entry->kind == SPN_TOOLCHAIN_BUILTIN) {
    session->toolchain.source = SPN_TOOLCHAIN_BUILTIN;
    session->toolchain.info = entry->info;
    session->toolchain.compiler = session->toolchain.info.compiler;
    session->toolchain.linker = session->toolchain.info.linker;
    session->toolchain.archiver = session->toolchain.info.archiver;
    session->toolchain.pkg = session->pkg;
  }
  else if (entry->kind == SPN_TOOLCHAIN_INDEX) {
    checkout_t* checkout = sp_str_ht_get(checkouts, entry->request.package);
    sp_assert(checkout);

    spn_toolchain_entry_t* toolchain = find_toolchain(checkout->pkg, host, target);
    sp_assert(toolchain->kind == SPN_TOOLCHAIN_INDEX);
    if (!toolchain) {
      log_toolchain_error(checkout->pkg, host, target);
      return SPN_TASK_ERROR;
    }

    // Add a unit for the data we need to download the tarball
    if (!sp_str_empty(toolchain->info.url)) {
      spn_toolchain_unit_t* unit = sp_alloc_type(spn_toolchain_unit_t);
      unit->session = session;
      unit->pkg = checkout->pkg;
      unit->url = toolchain->info.url;

      session->units.toolchain = unit;
    }

    // Mark down the toolchain for the session; we'll fill in the launchers
    // once we know where the toolchain's gonna be installed in the store
    session->toolchain.source = SPN_TOOLCHAIN_INDEX;
    session->toolchain.info = toolchain->info;
    session->toolchain.pkg = checkout->pkg;
  }

  // Load the manifests for each package and add a unit
  sp_str_ht_for_kv(checkouts, it) {
    checkout_t checkout = *it.val;
    spn_resolved_pkg_t* resolved = checkout.resolved;
    spn_index_rel_t* release = resolved->release;

    sp_om_insert(session->units.packages, checkout.pkg->qualified, SP_ZERO_STRUCT(spn_pkg_unit_t));
    spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
    spn_init_pkg_unit_for_session(session, unit, resolved->pkg, resolved->kind);

    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_PACKAGE,
      .sync_pkg = {
        .name = checkout.pkg->name,
        .kind = SPN_PACKAGE_KIND_INDEX,
        .url = release->source.url,
        .source_path = checkout.source,
        .time = checkout.elapsed,
      }
    });
  }

  // If we need to download a toolchain, point it at the unit for the corresponding
  // package. This is separate from where we resolve the toolchain because we don't
  // want to special case the toolchain's package unit.
  //
  // By setting up the paths here, the toolchain can be downloaded to the package's
  // work dir and decompressed to the store dir with no special paths.
  if (session->units.toolchain) {
    spn_toolchain_info_t toolchain = session->toolchain.info;
    spn_toolchain_unit_t* unit = session->units.toolchain;
    spn_pkg_unit_t* pkg = sp_om_get(session->units.packages, unit->pkg->qualified);

    sp_str_t store = pkg->ctx.paths.store;
    sp_str_t work = pkg->ctx.paths.work;

    // These are places in the cache
    unit->paths.store = store;
    unit->paths.work = work;
    unit->paths.stamp = sp_fs_join_path(work, sp_str_lit("download.stamp"));
    unit->paths.logs.build = sp_fs_join_path(work, sp_str_lit("build.log"));
    unit->paths.logs.jsonl = sp_fs_join_path(work, sp_str_lit("build.jsonl"));
    unit->logs.build = sp_io_writer_from_file(unit->paths.logs.build, SP_IO_WRITE_MODE_APPEND);
    unit->logs.jsonl = sp_io_writer_from_file(unit->paths.logs.jsonl, SP_IO_WRITE_MODE_APPEND);

    // These are the paths used to refer to the toolchain during compilation
    session->toolchain.compiler.program = sp_fs_join_path(store, toolchain.compiler.program);
    session->toolchain.compiler.args = toolchain.compiler.args;
    session->toolchain.linker.program = sp_fs_join_path(store, toolchain.linker.program);
    session->toolchain.linker.args = toolchain.linker.args;
    session->toolchain.archiver.program = sp_fs_join_path(store, toolchain.archiver.program);
    session->toolchain.archiver.args = toolchain.archiver.args;
  }

  // Populate session env with toolchain vars so that external build tools
  // (autoconf, make, cmake) automatically pick up the right compiler.
  sp_env_insert(&session->env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(session->toolchain.compiler));
  sp_env_insert(&session->env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(session->toolchain.archiver));
  sp_env_insert(&session->env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(session->toolchain.linker));

  // Load file dependencies directly from their manifests
  sp_ht_for_kv(app->package.deps, it) {
    spn_pkg_req_t* req = it.val;
    if (req->kind != SPN_PACKAGE_KIND_FILE) continue;

    sp_str_t manifest = req->file;
    if (sp_str_starts_with(manifest, SP_LIT("file://"))) {
      manifest = sp_str_strip_left(manifest, SP_LIT("file://"));
    }

    timers.pkg = sp_tm_start_timer();

    if (!sp_fs_exists(manifest)) {
      spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_SYNC_FAILED,
        .sync_failed = {
          .name = *it.key,
          .url = manifest,
          .error = sp_str_lit("manifest not found"),
        }
      });
      continue;
    }

    spn_pkg_t* pkg = sp_alloc_type(spn_pkg_t);
    spn_pkg_init(pkg, *it.key);
    spn_pkg_from_manifest(pkg, manifest);

    sp_om_insert(session->units.packages, *it.key, SP_ZERO_STRUCT(spn_pkg_unit_t));
    spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
    spn_init_pkg_unit_for_session(session, unit, pkg, SPN_PACKAGE_KIND_FILE);

    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_PACKAGE,
      .sync_pkg = {
        .name = *it.key,
        .kind = SPN_PACKAGE_KIND_FILE, // @spader Convert this to a string? Here, or can we express this in the schema?
        .url = manifest,
        .source_path = pkg->paths.root,
        .time = sp_tm_read_timer(&timers.pkg),
      }
    });
  }

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_END,
    .sync_end = {
      .num_synced = sp_str_ht_size(resolver->resolved),
      .time = sp_tm_read_timer(&timers.git),
    }
  });

  // @spader Elsewhere?
  sp_om_for(session->units.packages, it) {
    spn_pkg_t* pkg = sp_om_at(session->units.packages, it)->ctx.pkg;
    spn.tui.info.max_name = SP_MAX(spn.tui.info.max_name, pkg->name.len);
  }
  return SPN_TASK_CONTINUE;
}

spn_task_result_t spn_task_sync_update(spn_app_t* app) {
  return SPN_TASK_DONE;
}
