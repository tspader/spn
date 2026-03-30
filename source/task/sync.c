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
#include "session/session.h"
#include "task/task.h"
#include "toolchain/types.h"
#include <unistd.h>

typedef struct {
  spn_resolved_pkg_t* resolved;
  spn_pkg_t* pkg;
  sp_str_t manifest;
  sp_str_t script;
  sp_str_t source;
  u64 elapsed;
} checkout_t;

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

  // Resolve the toolchain — set session->toolchain.info before adding units,
  // because build hashes for index packages depend on it.
  session->toolchain = (spn_toolchain_t) {
    .info = sp_alloc_type(spn_toolchain_info_t),
    .compiler = { .program = sp_str_lit("gcc") },
    .linker = { .program = sp_str_lit("ld") },
    .archiver = { .program = sp_str_lit("ar") },
  };
  session->toolchain.info->compiler = (spn_toolchain_launcher_t) { .program = sp_str_lit("gcc") };
  session->toolchain.info->linker = (spn_toolchain_launcher_t) { .program = sp_str_lit("ld") };
  session->toolchain.info->archiver = (spn_toolchain_launcher_t) { .program = sp_str_lit("ar") };
  session->toolchain.info->driver = SPN_CC_DRIVER_GCC;
  session->toolchain.info->abi = SPN_ABI_GNU;

  spn_toolchain_entry_t* entry = app->config.toolchain;
  if (app->config.toolchain->kind == SPN_TOOLCHAIN_INDEX) {
    spn_toolchain_req_t* request = &app->config.toolchain->request;
    checkout_t* checkout = sp_str_ht_get(checkouts, request->package);
    sp_assert(checkout);

    spn_pkg_t* pkg = checkout->pkg;
    spn_toolchain_entry_t* entry = sp_om_at(checkout->pkg->toolchains, 0);
    sp_assert(entry);
    sp_assert(entry->kind == SPN_TOOLCHAIN_INLINE);

    spn_index_rel_t* release = checkout->resolved->release;
    sp_str_t store = sp_fs_join_path(pkg->paths.cache.store, release->source.rev);
    sp_str_t work = sp_fs_join_path(pkg->paths.cache.work, release->source.rev);

    spn_toolchain_info_t* info = &entry->info;

    session->toolchain = (spn_toolchain_t) {
      .info = info,
      .compiler = { sp_fs_join_path(store, info->compiler.program), info->compiler.args },
      .linker =   { sp_fs_join_path(store, info->linker.program), info->linker.args },
      .archiver = { sp_fs_join_path(store, info->archiver.program), info->archiver.args },
    };

    if (!sp_str_empty(info->url)) {
      session->units.toolchain = sp_alloc_type(spn_toolchain_unit_t);
      session->units.toolchain->url = info->url;
      session->units.toolchain->paths.store = store;
      session->units.toolchain->paths.work = work;
      session->units.toolchain->paths.stamp = sp_fs_join_path(work, sp_str_lit("download.stamp"));
      sp_fs_create_dir(store);
      sp_fs_create_dir(work);
    }
  }
  else if (entry->kind == SPN_TOOLCHAIN_INLINE) {
    *session->toolchain.info = entry->info;
    session->toolchain.compiler = session->toolchain.info->compiler;
    session->toolchain.linker = session->toolchain.info->linker;
    session->toolchain.archiver = session->toolchain.info->archiver;
  }

  // Load the manifests for each package and add a unit
  sp_str_ht_for_kv(checkouts, it) {
    checkout_t checkout = *it.val;
    spn_resolved_pkg_t* resolved = checkout.resolved;
    spn_index_rel_t* release = resolved->release;

    // Add a build unit to the session
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

  // Now that units exist, resolve the toolchain's store path from its build context
  if (!sp_str_empty(session->pkg->toolchain) && !sp_str_empty(session->toolchain.info->url)) {
    spn_pkg_unit_t* tc_unit = sp_om_get(session->units.packages, session->pkg->toolchain);
    sp_assert(tc_unit);
    session->toolchain.root = tc_unit->ctx.paths.store;
    session->toolchain.stamp = sp_fs_join_path(tc_unit->ctx.paths.store, sp_str_lit(".toolchain.stamp"));
  }

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
