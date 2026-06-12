#include "app/types.h"
#include "forward/types.h"
#include "intern/intern.h"
#include "log/log.h"
#include "ordered_map.h"
#include "session/types.h"
#include "target/types.h"
#include "task/types.h"

#include "enum/enum.h"
#include "error/types.h"
#include "filter/filter.h"
#include "pkg/id.h"
#include "session/session.h"
#include "sp/sp_glob.h"
#include "target/select.h"
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


static spn_err_t set_target_kind(spn_session_t* session, spn_target_unit_t* target) {
  spn_target_info_t* info = target->info;

  switch (info->kind) {
    case SPN_TARGET_EXE:
    case SPN_TARGET_SCRIPT:
    case SPN_TARGET_TEST: {
      target->kind = SPN_CC_OUTPUT_EXE;
      return SPN_OK;
    }
    case SPN_TARGET_LIB: {
      spn_kind_query_t query = {
        .config = spn_session_config_kind(session, target->pkg->info->name),
        .linkage = session->profile.linkage,
      };

      if (spn_target_select_lib_kind(info, query, &target->lib_kind)) {
        sp_str_t requested = spn_pkg_linkage_to_str(query.config.some ? query.config.value : query.linkage);
        sp_str_t requester = query.config.some ? sp_str_lit("the root manifest") : sp_str_lit("the profile");
        spn_log_error(
          "{:fg brightcyan} doesn't support {:fg brightyellow} ({} requested it)",
          SP_FMT_STR(target->pkg->info->name),
          SP_FMT_STR(requested),
          SP_FMT_STR(requester)
        );
        return SPN_ERROR;
      }

      switch (target->lib_kind) {
        case SPN_LIB_KIND_STATIC: target->kind = SPN_CC_OUTPUT_STATIC_LIB; break;
        case SPN_LIB_KIND_SHARED: target->kind = SPN_CC_OUTPUT_SHARED_LIB; break;
        case SPN_LIB_KIND_SOURCE: target->kind = SPN_CC_OUTPUT_OBJECT; break;
        case SPN_LIB_KIND_NONE: break;
      }
      return SPN_OK;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_task_result_t spn_task_create_units(spn_app_t* app) {
  spn_session_t* session = &app->session;
  sp_str_om_for(session->units.packages, it) {
    spn_pkg_unit_t* pkg = sp_str_om_at(session->units.packages, it);
    spn_pkg_info_t* info = pkg->info;
    spn_loaded_pkg_t* loaded = sp_str_ht_get(session->packages, info->qualified);

    // The target filter only applies to the root package; dependencies contribute
    // exactly their lib targets no matter what we were asked to build.
    sp_da(spn_target_info_t*) targets = SP_NULLPTR;
    sp_str_om_for(info->libs, it) {
      sp_da_push(targets, sp_str_om_at(info->libs, it));
    }
    if (loaded->source == SPN_PKG_SOURCE_ROOT) {
      sp_str_om_for(info->exes, it) {
        sp_da_push(targets, sp_str_om_at(info->exes, it));
      }
      sp_str_om_for(info->scripts, it) {
        sp_da_push(targets, sp_str_om_at(info->scripts, it));
      }
      sp_str_om_for(info->tests, it) {
        sp_da_push(targets, sp_str_om_at(info->tests, it));
      }
    }

    sp_da_for(targets, it) {
      if (loaded->source == SPN_PKG_SOURCE_ROOT && !spn_target_filter_pass(&session->filter, targets[it])) {
        continue;
      }

      spn_target_unit_t* target = spn_session_add_target(session, pkg, targets[it]);
      spn_try_as(set_target_kind(session, target), SPN_TASK_ERROR);
    }
  }

  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* unit = sp_om_at(session->units.targets, it);
    sp_da_for(unit->info->deps, j) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[j]);
      struct {
        spn_pkg_unit_t* pkg;
        spn_target_unit_t* target;
      } candidates = {
        .pkg = spn_session_find_pkg_by_qualified(session, qualified),
        .target = spn_session_find_target_in_pkg(session, unit->pkg, unit->info->deps[j])
      };

      if (candidates.pkg) {
        sp_da_push(unit->deps.package, candidates.pkg);
      }
      else if (candidates.target) {
        sp_da_push(unit->deps.target, candidates.target);
      }
      else {
        spn_log_error("failed to find {:fg cyan} as a package or target", SP_FMT_STR(unit->info->deps[j]));
      }
    }
  }


  // Now that the configure phase is done, everything about the build is static. We
  // can go through every target and resolve file globs into object files which need
  // to be compiled.
  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* target = sp_om_at(session->units.targets, it);
    spn_pkg_unit_t* pkg = target->pkg;

    // Source libs are consumed as source; we never compile them ourselves
    if (target->lib_kind == SPN_LIB_KIND_SOURCE) continue;

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

      if (!sp_str_om_has(session->units.objects, file)) {
        sp_str_om_insert(session->units.objects, file, ((spn_compile_unit_t) {
          .package = pkg,
          .target = target,
          .session = target->session,
          .paths = {
            .object = object_path,
            .file = file,
          },
        }));
      }

      spn_compile_unit_t* object = sp_str_om_get(session->units.objects, file);
      sp_da_push(pkg->objects, object);
      sp_da_push(target->objects, object);
    }
  }

  return SPN_TASK_DONE;
}

