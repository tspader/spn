#include "app/types.h"
#include "ctx/types.h"

#include "app/app.h"
#include "index/cache.h"
#include "intern/intern.h"
#include "lock/lock.h"
#include "log/log.h"
#include "pkg/id.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "task/task.h"

static spn_resolved_pkg_t* spn_update_find_resolved(spn_resolve_t resolve, sp_str_t qualified) {
  spn_resolved_pkg_t* found = SP_NULLPTR;
  sp_ht_for_kv(resolve, it) {
    if (!sp_str_equal(spn_intern_str(it.val->id.qualified), qualified)) {
      continue;
    }
    if (!found || spn_semver_ge(it.val->id.version, found->id.version)) {
      found = it.val;
    }
  }
  return found;
}

static u32 spn_update_report_changes(spn_app_t* app, sp_mem_t mem) {
  spn_lock_file_t fresh = spn_build_lock_file(mem, app->session.intern, app->session.resolve, &app->package);
  spn_lock_file_t* old = app->lock.some ? &app->lock.value : SP_NULLPTR;

  u32 num_changed = 0;
  sp_ht_for_kv(fresh.entries, it) {
    spn_lock_entry_t* entry = it.val;
    spn_lock_entry_t* locked = old ? sp_ht_getp(old->entries, entry->name) : SP_NULLPTR;

    if (!locked) {
      spn_log_info("added {.cyan} {.green}", SP_FMT_STR(entry->name), SP_FMT_STR(spn_semver_to_str(mem, entry->version)));
      num_changed++;
    }
    else if (!spn_semver_eq(locked->version, entry->version)) {
      spn_log_info(
        "updated {.cyan} {.yellow} -> {.green}",
        SP_FMT_STR(entry->name),
        SP_FMT_STR(spn_semver_to_str(mem, locked->version)),
        SP_FMT_STR(spn_semver_to_str(mem, entry->version))
      );
      num_changed++;
    }
    else if (locked->kind != entry->kind || !sp_str_equal(locked->commit, entry->commit)) {
      spn_log_info("changed {.cyan} {.green}", SP_FMT_STR(entry->name), SP_FMT_STR(spn_semver_to_str(mem, entry->version)));
      num_changed++;
    }
  }

  if (old) {
    sp_ht_for_kv(old->entries, it) {
      if (!sp_ht_getp(fresh.entries, it.val->name)) {
        spn_log_info("removed {.cyan}", SP_FMT_STR(it.val->name));
        num_changed++;
      }
    }
  }

  return num_changed;
}

static void spn_update_report_incompatible(spn_app_t* app, sp_mem_t mem) {
  spn_index_cache_t cache = SP_ZERO_INITIALIZE();
  spn_index_cache_init(&cache, spn.heap, spn.intern, &spn.indexes);

  sp_da_for(app->package.deps, it) {
    spn_requested_pkg_t* dep = &app->package.deps[it];
    if (dep->source != SPN_PKG_SOURCE_INDEX) {
      continue;
    }

    spn_resolved_pkg_t* resolved = spn_update_find_resolved(app->session.resolve, dep->qualified);
    if (!resolved) {
      continue;
    }

    spn_index_pkg_t* pkg = spn_index_cache_get_package(&cache, spn_pkg_name_from_qualified(dep->qualified));
    if (!pkg) {
      continue;
    }

    sp_da_rfor(pkg->releases, release_it) {
      spn_index_rel_t* release = &pkg->releases[release_it];
      if (release->yanked) {
        continue;
      }
      if (spn_semver_ge(release->version, resolved->id.version) && !spn_semver_in_range(release->version, dep->index.range)) {
        spn_log_info(
          "{.cyan} {.green} (latest {.yellow} is semver incompatible)",
          SP_FMT_STR(dep->qualified),
          SP_FMT_STR(spn_semver_to_str(mem, resolved->id.version)),
          SP_FMT_STR(spn_semver_to_str(mem, release->version))
        );
      }
      break;
    }
  }
}

spn_task_step_t spn_task_update(spn_app_t* app) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  u32 num_changed = spn_update_report_changes(app, scratch.mem);
  spn_update_report_incompatible(app, scratch.mem);

  if (!num_changed) {
    spn_log_info("up to date");
  }

  spn_app_update_lock_file(app);

  sp_mem_end_scratch(scratch);
  return spn_task_done();
}
