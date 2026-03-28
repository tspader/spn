#include "app/app.h"
#include "app/types.h"
#include "ctx/ctx.h"
#include "ctx/types.h"
#include "err.h"
#include "event/event.h"
#include "git/cache.h"
#include "log/log.h"
#include "pkg/id.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "session/session.h"
#include "task/task.h"

static void add_pkg_unit(spn_session_t* session, sp_str_t qualified_name, spn_resolved_pkg_t* resolved) {
  sp_om_insert(session->units.packages, qualified_name, SP_ZERO_STRUCT(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  spn_init_pkg_unit_for_session(session, unit, resolved->pkg, resolved->kind, resolved->version);
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
    sp_tm_timer_t all;
    sp_tm_timer_t pkg;
  } timers = sp_zero_initialize();

  timers.all = sp_tm_start_timer();

  // Load file dependencies directly from their manifests
  sp_ht_for_kv(app->package.deps, dep_it) {
    spn_pkg_req_t* req = dep_it.val;
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
          .name = *dep_it.key,
          .url = manifest,
          .error = sp_str_lit("manifest not found"),
        }
      });
      continue;
    }

    spn_pkg_t* pkg = sp_alloc_type(spn_pkg_t);
    spn_pkg_init(pkg, *dep_it.key);
    spn_pkg_from_manifest(pkg, manifest);

    spn_resolved_pkg_t file_resolved = {
      .pkg = pkg,
      .kind = SPN_PACKAGE_KIND_FILE,
      .version = pkg->version,
    };
    add_pkg_unit(session, *dep_it.key, &file_resolved);

    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_PACKAGE,
      .sync_pkg = {
        .name = *dep_it.key,
        .kind = SPN_PACKAGE_KIND_FILE, // @spader Convert this to a string? Here, or can we express this in the schema?
        .url = manifest,
        .source_path = pkg->paths.root,
        .time = sp_tm_read_timer(&timers.pkg),
      }
    });
  }

  // Load index dependencies via git cache
  spn_git_cache_t cache = SP_ZERO_INITIALIZE();
  spn_git_cache_init(&cache, spn.paths.source);

  sp_str_ht_for_kv(resolver->resolved, it) {
    spn_resolved_pkg_t* resolved = it.val;
    if (resolved->kind != SPN_PACKAGE_KIND_INDEX) {
      continue;
    }

    spn_err_t err = SPN_OK;
    spn_index_rel_t* release = resolved->release;

    timers.pkg = sp_tm_start_timer();

    struct {
      spn_git_checkout_t* manifest;
      spn_git_checkout_t* source;
    } checkouts = sp_zero_initialize();

    struct {
      sp_str_t manifest;
      sp_str_t script;
    } paths = sp_zero_initialize();

    // If the manifest is in a different repo than the source, check that out first
    if (!sp_str_empty(release->manifest.url)) {
      spn_git_checkout_id_t id = {
        .url = release->manifest.url,
        .rev = release->manifest.rev,
        .dir = release->manifest.dir
      };
      if (spn_git_cache_ensure_checkout(&cache, id, &checkouts.manifest)) {
        spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_SYNC_FAILED,
          .sync_failed = {
            .name = *it.key,
            .url = release->manifest.url,
            .error = sp_str_lit("manifest checkout failed"),
          }
        });
        return SPN_TASK_ERROR;
      }
    }

    // If the package has source code, clone it
    if (!sp_str_empty(release->source.url)) {
      spn_git_checkout_id_t id = {
        .url = release->source.url,
        .rev = release->source.rev,
        .dir = release->source.dir
      };
      if (spn_git_cache_ensure_checkout(&cache, id, &checkouts.source)) {
        spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_SYNC_FAILED,
          .sync_failed = {
            .name = *it.key,
            .url = release->source.url,
            .error = sp_str_lit("source checkout failed"),
          }
        });
        return SPN_TASK_ERROR;
      }
    }

    if (checkouts.manifest) {
      paths.manifest = sp_fs_join_path(checkouts.manifest->path, release->paths.manifest);
      paths.script = sp_fs_join_path(checkouts.manifest->path, release->paths.script);
    }
    else {
      paths.manifest = sp_fs_join_path(checkouts.source->path, release->paths.manifest);
      paths.script = sp_fs_join_path(checkouts.source->path, release->paths.script);
    }

    if (sp_fs_exists(paths.manifest)) {
      sp_assert_fmt(
        sp_fs_exists(paths.manifest),
        "manifest didn't exist; pkg = {}, path = {}, checkout = {}",
        SP_FMT_STR(spn_pkg_id_to_qualified_name(release->id)),
        SP_FMT_STR(paths.manifest),
        SP_FMT_PTR(checkouts.manifest)
      );
    }

    // Read the manifest from disk
    spn_pkg_t* pkg = sp_alloc_type(spn_pkg_t);
    spn_pkg_init(pkg, release->id.name);
    spn_pkg_from_manifest(pkg, paths.manifest);
    spn_pkg_add_version_ex(pkg, resolved->version, release->source.rev);

    pkg->kind = SPN_PACKAGE_KIND_INDEX;
    pkg->paths.root = checkouts.source->path;
    pkg->paths.script = paths.script;
    pkg->paths.manifest = paths.manifest;
    pkg->paths.cache.source = checkouts.source->path;
    pkg->paths.cache.work = sp_fs_join_path(spn.paths.build, *it.key);
    pkg->paths.cache.store = sp_fs_join_path(spn.paths.store, *it.key);

    resolved->pkg = pkg;

    // Add a build unit to the session
    add_pkg_unit(session, *it.key, resolved);

    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_PACKAGE,
      .sync_pkg = {
        .name = *it.key,
        .kind = SPN_PACKAGE_KIND_INDEX,
        .url = release->source.url,
        .source_path = checkouts.source->path,
        .time = sp_tm_read_timer(&timers.pkg),
      }
    });
  }

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_END,
    .sync_end = {
      .num_synced = sp_str_ht_size(resolver->resolved),
      .time = sp_tm_read_timer(&timers.all),
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
