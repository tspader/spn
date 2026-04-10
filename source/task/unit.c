#include "app/types.h"
#include "forward/types.h"
#include "intern/intern.h"
#include "ordered_map.h"
#include "session/types.h"
#include "target/types.h"
#include "task/types.h"

#include "filter/filter.h"
#include "pkg/id.h"
#include "session/session.h"
#include "sp/sp_glob.h"
#include "unit/types.h"

static bool has_source_file(sp_da(sp_str_t) source, sp_str_t path) {
  sp_da_for(source, it) {
    if (sp_str_equal(source[it], path)) {
      return true;
    }
  }

  return false;
}

static void collect_source_glob(sp_str_t root, sp_str_t pattern, sp_da(sp_str_t)* source) {
  sp_glob_t* glob = sp_glob_new_str(pattern);
  if (!glob) {
    return;
  }

  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(root);
  sp_da(sp_str_t) matches = SP_NULLPTR;

  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_file(entry->file_path)) {
      continue;
    }

    sp_str_t relative = sp_str_strip_left(entry->file_path, root);
    relative = sp_str_strip_left(relative, sp_str_lit("/"));
    if (!sp_glob_match(glob, relative)) {
      continue;
    }
    if (has_source_file(matches, relative)) {
      continue;
    }

    sp_da_push(matches, relative);
  }

  sp_dyn_array_sort(matches, sp_str_sort_kernel_alphabetical);

  sp_da_for(matches, it) {
    if (has_source_file(*source, matches[it])) {
      continue;
    }

    sp_da_push(*source, matches[it]);
  }
}

static sp_da(sp_str_t) collect_target_source(spn_pkg_unit_t* pkg, spn_target_unit_t* target) {
  sp_da(sp_str_t) source = SP_NULLPTR;

  sp_da_for(target->info->source, it) {
    sp_str_t path = target->info->source[it];
    if (sp_fs_is_glob(path)) {
      collect_source_glob(pkg->paths.source, path, &source);
      continue;
    }
    if (has_source_file(source, path)) {
      continue;
    }

    sp_da_push(source, path);
  }

  return source;
}


spn_task_result_t spn_task_create_units(spn_app_t* app) {
  spn_session_t* session = &app->session;
  sp_str_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_str_om_at(session->units.packages, it);
    spn_pkg_info_t* pkg = unit->info;
    spn_loaded_pkg_t* loaded = sp_str_ht_get(session->packages, pkg->qualified);

    sp_da(spn_target_info_t*) targets;
    sp_str_om_for(pkg->exes, it) {
      sp_da_push(targets, sp_str_om_at(pkg->exes, it));
    }
    sp_str_om_for(pkg->libs, it) {
      sp_da_push(targets, sp_str_om_at(pkg->libs, it));
    }
    if (loaded->source == SPN_PKG_SOURCE_ROOT) {
      sp_str_om_for(pkg->scripts, it) {
        sp_da_push(targets, sp_str_om_at(pkg->scripts, it));
      }
      sp_str_om_for(pkg->tests, it) {
        sp_da_push(targets, sp_str_om_at(pkg->tests, it));
      }
    }

    sp_da_for(targets, it) {
      spn_target_info_t* target = targets[it];
      if (spn_target_filter_pass(&session->filter, target)) {
        spn_session_add_target(session, unit, target);
      }
    }
  }

  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* unit = sp_om_at(session->units.targets, it);
    sp_da_for(unit->info->deps, j) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[j]);
      spn_pkg_unit_t* pkg = spn_session_find_pkg_by_qualified(session, qualified);
      spn_target_unit_t* target = spn_session_find_target_in_pkg(session, unit->pkg, unit->info->deps[j]);
      sp_intern_id_t id = sp_intern_get_or_insert(session->intern, qualified);
      struct {
        spn_pkg_unit_id_t pkg;
        spn_target_unit_id_t target;
      } ids = {
        .pkg = { id },
        .target = { unit->pkg->id, id },
      };

      if (sp_om_has(session->units.packages, ids.pkg)) {
        spn_pkg_unit_t* pkg = sp_om_get(session->units.packages, ids.pkg);
        sp_da_push(unit->deps.package, pkg);
      }
      else if (sp_om_has(session))
    }
  }

  sp_str_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_str_om_at(session->units.packages, it);
    spn_pkg_info_t* info = unit->info;
    spn_loaded_pkg_t* loaded = sp_str_ht_get(session->packages, info->qualified);

    switch (loaded->source) {
      case SPN_PKG_SOURCE_ROOT: {
        sp_str_om_for(info->exes, it) {
          spn_target_info_t* target = sp_str_om_at(info->exes, it);
          if (!spn_target_filter_pass(&session->filter, target)) {
            continue;
          }


        }
      }
    }

  }


  // Now that the configure phase is done, everything about the build is static. We
  // can go through every target and resolve file globs into object files which need
  // to be compiled.
  sp_str_om_for(session->units.targets, it) {
    spn_target_unit_t* target = sp_str_om_at(session->units.targets, it);
    spn_pkg_unit_t* pkg = target->pkg;

    sp_da(sp_str_t) source = collect_target_source(pkg, target);
    sp_da_for(source, j) {
      sp_str_t relative = source[j];
      sp_str_t file = sp_fs_join_path(pkg->paths.source, relative);
      sp_str_t extension = sp_fs_get_ext(relative);
      sp_str_t stem = relative;
      if (!sp_str_empty(extension)) {
        stem = sp_str_prefix(relative, relative.len - extension.len - 1);
      }

      sp_str_t object_path = sp_fs_join_path(target->paths.object, sp_format("{}.o", SP_FMT_STR(stem)));

      if (!sp_str_om_has(pkg->objects, file)) {
        sp_str_om_insert(pkg->objects, file, ((spn_compile_unit_t) {
          .package = pkg,
          .target = target,
          .session = target->session,
          .paths = {
            .object = object_path,
            .file = file,
          },
        }));
      }

      spn_compile_unit_t* object = sp_str_om_get(pkg->objects, file);
      sp_da_push(target->objects, object);
    }
  }

  return SPN_TASK_DONE;
}

