#include "sp.h"
#include "sp/macro.h"
#include "app/types.h"
#include "forward/types.h"
#include "intern/intern.h"
#include "log/log.h"
#include "sp/sp_om.h"
#include "session/types.h"
#include "target/types.h"
#include "task/types.h"

#include "ctx/types.h"
#include "enum/enum.h"
#include "error/types.h"
#include "event/event.h"
#include "event/types.h"
#include "filter/filter.h"
#include "pkg/id.h"
#include "session/invocation.h"
#include "session/session.h"
#include "sp/sp_glob.h"
#include "target/mutate.h"
#include "target/select.h"
#include "task/build/build.h"
#include "toolchain/toolchain.h"
#include "unit/types.h"

static bool has_source_file(sp_da(sp_str_t) source, sp_str_t path) {
  sp_da_for(source, it) {
    if (sp_str_equal(source[it], path)) {
      return true;
    }
  }

  return false;
}

static sp_str_t glob_literal_dir(sp_str_t pattern) {
  u32 cut = 0;
  for (u32 i = 0; i < pattern.len; i++) {
    c8 c = pattern.data[i];
    if (c == '*' || c == '?' || c == '[' || c == '{') break;
    if (c == '/') cut = i;
  }
  return sp_str_sub(pattern, 0, cut);
}

static void collect_source_glob(sp_mem_t mem, sp_str_t root, sp_str_t pattern, sp_da(sp_str_t)* source) {
  sp_glob_t* glob = sp_glob_new_str(mem, pattern);
  if (!glob) {
    return;
  }

  sp_str_t sub = glob_literal_dir(pattern);
  sp_str_t scan = sp_str_empty(sub) ? root : sp_fs_join_path(mem, root, sub);
  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(mem, scan);
  sp_da(sp_str_t) matches = sp_da_new(mem, sp_str_t);

  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_file(entry->path)) {
      continue;
    }

    sp_str_t relative = sp_str_strip_left(entry->path, root);
    relative = sp_str_strip_left(relative, sp_str_lit("/"));
    if (!sp_glob_match(glob, relative)) {
      continue;
    }
    if (has_source_file(matches, relative)) {
      continue;
    }

    sp_da_push(matches, relative);
  }

  sp_da_sort(matches, sp_str_sort_kernel_alphabetical);

  sp_da_for(matches, it) {
    if (has_source_file(*source, matches[it])) {
      continue;
    }

    sp_da_push(*source, matches[it]);
  }
}

static sp_da(sp_str_t) collect_target_source(sp_mem_t mem, spn_pkg_unit_t* pkg, spn_target_unit_t* target) {
  sp_da(sp_str_t) source = sp_da_new(mem, sp_str_t);

  sp_da_for(target->info->source, it) {
    sp_str_t path = target->info->source[it];
    if (sp_fs_is_glob(path)) {
      collect_source_glob(mem, pkg->paths.source, path, &source);
      continue;
    }
    if (has_source_file(source, path)) {
      continue;
    }

    sp_da_push(source, path);
  }

  return source;
}


static bool exe_name_reserved(sp_str_t name) {
  return sp_str_equal_cstr(name, "store") || sp_str_equal_cstr(name, "work") || sp_str_equal_cstr(name, "test");
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
      // Unlinked libs don't participate in linkage selection: nothing consumes
      // them at link time, so the profile has no say in what they produce. A
      // multi-kind unlinked lib resolves by spn_linkage_set_default's
      // preference order, not the profile. Object libs are always unlinked;
      // the manifest loader rejects object mixed with linkable kinds.
      if (spn_linkage_set_has(info->linkages, SPN_LIB_KIND_OBJECT) || info->no_link) {
        target->lib_kind = spn_linkage_set_default(info->linkages);
      }
      else {
        spn_kind_query_t query = {
          .config = spn_session_config_kind(session, target->pkg->info->name),
          .linkage = session->profile.linkage,
        };

        if (spn_target_select_lib_kind(info, query, &target->lib_kind)) {
          sp_str_t requested = spn_linkage_to_str(query.config.some ? query.config.value : query.linkage);
          sp_str_t requester = query.config.some ? sp_str_lit("the root manifest") : sp_str_lit("the profile");
          spn_log_error(
            "{.cyan} doesn't support {.yellow} ({} requested it)",
            SP_FMT_STR(target->pkg->info->name),
            SP_FMT_STR(requested),
            SP_FMT_STR(requester)
          );
          return SPN_ERROR;
        }
      }

      switch (target->lib_kind) {
        case SPN_LIB_KIND_STATIC: target->kind = SPN_CC_OUTPUT_STATIC_LIB; break;
        case SPN_LIB_KIND_SHARED: target->kind = SPN_CC_OUTPUT_SHARED_LIB; break;
        // Source and object libs share an output kind; anything that needs to
        // tell them apart must branch on lib_kind, not kind
        case SPN_LIB_KIND_SOURCE:
        case SPN_LIB_KIND_OBJECT: target->kind = SPN_CC_OUTPUT_OBJECT; break;
        case SPN_LIB_KIND_NONE: break;
      }
      return SPN_OK;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

static spn_pkg_unit_t* find_dep_unit(spn_session_t* session, spn_pkg_unit_t* pkg, sp_str_t qualified) {
  const spn_dep_kind_t kinds [] = { SPN_DEP_KIND_PACKAGE, SPN_DEP_KIND_TEST, SPN_DEP_KIND_BUILD };
  sp_carr_for(kinds, it) {
    spn_pkg_unit_t* unit = spn_session_find_dep(session, pkg, qualified, kinds[it]);
    if (unit) {
      return unit;
    }
  }
  return SP_NULLPTR;
}

spn_task_step_t spn_task_create_units(spn_app_t* app) {
  spn_session_t* session = &app->session;
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* pkg = sp_om_at(session->units.packages, it);
    spn_pkg_info_t* info = pkg->info;
    spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, pkg->id);

    // The target filter only applies to the root package; dependencies contribute
    // exactly their lib targets no matter what we were asked to build.
    sp_da(spn_target_info_t*) targets = sp_da_new(session->mem, spn_target_info_t*);
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

      bool staged_at_root = targets[it]->kind == SPN_TARGET_EXE || targets[it]->kind == SPN_TARGET_SCRIPT;
      if (loaded->source == SPN_PKG_SOURCE_ROOT && staged_at_root && exe_name_reserved(targets[it]->name)) {
        spn_log_error(
          "{.cyan} names an executable {.yellow}, which collides with a build output directory (store, work, test)",
          SP_FMT_STR(info->name),
          SP_FMT_STR(targets[it]->name)
        );
        return spn_task_fail(SPN_ERROR);
      }

      spn_target_unit_t* target = spn_session_add_target(session, pkg, targets[it]);
      if (set_target_kind(session, target)) return spn_task_fail(SPN_ERROR);
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
        .pkg = find_dep_unit(session, unit->pkg, qualified),
        .target = spn_session_find_target_in_pkg(session, unit->pkg, unit->info->deps[j])
      };

      if (candidates.pkg) {
        sp_da_push(unit->deps.package, candidates.pkg);
      }
      else if (candidates.target) {
        sp_da_push(unit->deps.target, candidates.target);
      }
      else {
        spn_log_error("failed to find {.cyan} as a package or target", SP_FMT_STR(unit->info->deps[j]));
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

    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_da(sp_str_t) source = collect_target_source(scratch.mem, pkg, target);

    sp_da_for(source, j) {
      sp_str_t relative = source[j];
      sp_str_t file = sp_fs_join_path(app->session.mem, pkg->paths.source, relative);

      spn_lang_t lang = spn_lang_from_path(relative);

      // Object libs publish their objects as artifacts; everyone else keeps
      // them as intermediates. The object name keeps the full source-relative
      // path, extension included, so colliding sources stay distinct.
      sp_str_t object_dir = target->lib_kind == SPN_LIB_KIND_OBJECT ?
        target->paths.lib :
        target->paths.object;
      sp_str_t object_path = sp_fs_join_path(app->session.mem, object_dir, sp_fmt(scratch.mem, "{}.o", SP_FMT_STR(relative)).value);

      if (!sp_str_om_has(session->units.objects, file)) {
        sp_str_om_insert(session->units.objects, file, ((spn_compile_unit_t) {
          .package = pkg,
          .target = target,
          .session = target->session,
          .lang = lang,
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
    sp_mem_end_scratch(scratch);
  }

  spn_toolchain_t* toolchain = session->units.toolchain->toolchain;
  if (!spn_toolchain_has_cxx(toolchain)) {
    sp_om_for(session->units.objects, it) {
      spn_compile_unit_t* object = sp_om_at(session->units.objects, it);
      if (object->lang != SPN_LANG_CXX) {
        continue;
      }

      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR,
        .err = {
          .kind = SPN_ERR_TOOLCHAIN_NO_CXX,
          .toolchain = { .name = toolchain->name },
        },
      });
      return spn_task_fail(SPN_ERROR);
    }
  }

  spn_session_build_invocations(session);
  spn_build_link_invocations(session);
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));

  return spn_task_done();
}

