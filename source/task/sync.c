#include "app/app.h"
#include "ctx/ctx.h"
#include "event/event.h"
#include "git/cache.h"
#include "log/log.h"
#include "pkg/id.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "session/session.h"

static void add_pkg_unit(spn_session_t* session, sp_str_t qualified_name, spn_resolved_pkg_t* resolved) {
  sp_om_insert(session->units.packages, qualified_name, SP_ZERO_STRUCT(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  spn_init_pkg_unit_for_session(session, unit, resolved->pkg, resolved->kind, resolved->version);
}

void spn_task_sync_init(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_resolver_t* resolver = app->resolver;
  spn_pkg_unit_t* root = spn_session_find_root(session);

  // count file vs index deps
  u32 num_file = 0, num_index = 0;
  sp_ht_for_kv(app->package.deps, count_it) {
    if (count_it.val->kind == SPN_PACKAGE_KIND_FILE) num_file++;
  }
  sp_str_ht_for_kv(resolver->resolved, count_it2) {
    if (count_it2.val->kind == SPN_PACKAGE_KIND_INDEX && count_it2.val->release) num_index++;
  }

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_START,
    .sync_start = {
      .num_packages = num_file + num_index,
      .num_index = num_index,
      .num_file = num_file,
    }
  });

  sp_tm_timer_t phase_timer = sp_tm_start_timer();
  u32 num_synced = 0;

  // Load file dependencies directly from their manifests
  sp_ht_for_kv(app->package.deps, dep_it) {
    spn_pkg_req_t* req = dep_it.val;
    if (req->kind != SPN_PACKAGE_KIND_FILE) continue;

    sp_str_t manifest = req->file;
    if (sp_str_starts_with(manifest, SP_LIT("file://"))) {
      manifest = sp_str_strip_left(manifest, SP_LIT("file://"));
    }

    sp_tm_timer_t pkg_timer = sp_tm_start_timer();

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
    num_synced++;

    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_PACKAGE,
      .sync_pkg = {
        .name = *dep_it.key,
        .kind = SPN_PACKAGE_KIND_FILE, // @spader Convert this to a string? Here, or can we express this in the schema?
        .url = manifest,
        .source_path = pkg->paths.root,
        .time = sp_tm_read_timer(&pkg_timer),
      }
    });
  }

  // Load index dependencies via git cache
  spn_git_cache_t cache = SP_ZERO_INITIALIZE();
  spn_git_cache_init(&cache, spn.paths.source);

  sp_str_ht_for_kv(resolver->resolved, it) {
    spn_resolved_pkg_t* resolved = it.val;

    if (resolved->kind != SPN_PACKAGE_KIND_INDEX || !resolved->release) {
      continue;
    }

    spn_index_rel_t* rel = resolved->release;
    sp_tm_timer_t pkg_timer = sp_tm_start_timer();

    spn_git_checkout_t* checkout = SP_NULLPTR;
    spn_err_t err = spn_git_cache_ensure_checkout(&cache, (spn_git_checkout_id_t) {
      .url = rel->source.url,
      .rev = rel->source.rev,
      .dir = rel->source.dir,
    }, &checkout);

    if (err != SPN_OK) {
      spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_SYNC_FAILED,
        .sync_failed = {
          .name = *it.key,
          .url = rel->source.url,
          .error = sp_str_lit("git checkout failed"),
        }
      });
      continue;
    }

    sp_str_t checkout_path = sp_str_copy(checkout->path);

    sp_str_t manifest_base = checkout_path;
    if (!sp_str_empty(rel->manifest.url)) {
      spn_git_checkout_t* manifest_checkout = SP_NULLPTR;
      err = spn_git_cache_ensure_checkout(&cache, (spn_git_checkout_id_t) {
        .url = rel->manifest.url,
        .rev = rel->manifest.rev,
        .dir = rel->manifest.dir,
      }, &manifest_checkout);

      if (err != SPN_OK) {
        spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_SYNC_FAILED,
          .sync_failed = {
            .name = *it.key,
            .url = rel->manifest.url,
            .error = sp_str_lit("manifest checkout failed"),
          }
        });
        continue;
      }

      manifest_base = sp_str_copy(manifest_checkout->path);
    }

    sp_str_t manifest_path = sp_fs_join_path(manifest_base, rel->paths.manifest);
    sp_str_t script_path = sp_fs_join_path(manifest_base, rel->paths.script);

    spn_pkg_t* pkg = sp_alloc_type(spn_pkg_t);
    spn_pkg_init(pkg, rel->id.name);

    if (sp_fs_exists(manifest_path)) {
      spn_pkg_from_manifest(pkg, manifest_path);
    }

    pkg->kind = SPN_PACKAGE_KIND_INDEX;
    pkg->paths.root = checkout_path;
    pkg->paths.script = script_path;
    pkg->paths.manifest = manifest_path;
    pkg->paths.cache.source = checkout_path;
    pkg->paths.cache.work = sp_fs_join_path(spn.paths.build, *it.key);
    pkg->paths.cache.store = sp_fs_join_path(spn.paths.store, *it.key);

    spn_pkg_add_version_ex(pkg, resolved->version, rel->source.rev);

    resolved->pkg = pkg;
    add_pkg_unit(session, *it.key, resolved);
    num_synced++;

    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_SYNC_PACKAGE,
      .sync_pkg = {
        .name = *it.key,
        .kind = SPN_PACKAGE_KIND_INDEX,
        .url = rel->source.url,
        .source_path = checkout_path,
        .time = sp_tm_read_timer(&pkg_timer),
      }
    });
  }

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_SYNC_END,
    .sync_end = {
      .num_synced = num_synced,
      .time = sp_tm_read_timer(&phase_timer),
    }
  });

  sp_om_for(session->units.packages, it) {
    spn_pkg_t* pkg = sp_om_at(session->units.packages, it)->ctx.pkg;
    spn.tui.info.max_name = SP_MAX(spn.tui.info.max_name, pkg->name.len);
  }
}

spn_task_result_t spn_task_sync_update(spn_app_t* app) {
  return SPN_TASK_DONE;
}
