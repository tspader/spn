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
#include "external/wasm/wasm.h"
#include "filter/filter.h"
#include "pkg/id.h"
#include "pkg/pkg.h"
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
    case SPN_TARGET_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_BUILD_METAPROGRAM: {
      target->kind = SPN_CC_OUTPUT_REACTOR;
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
          .linkage = target->build->profile.linkage,
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

static spn_err_t ensure_target(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info, spn_target_unit_t** result) {
  spn_target_unit_t* target = spn_session_find_target_in_pkg(session, pkg, info->name);
  if (target && target->info != info) {
    spn_log_error(
      "{.cyan} declares a target {.yellow}, which collides with another target of the same name",
      SP_FMT_STR(pkg->info->name),
      SP_FMT_STR(info->name)
    );
    return SPN_ERROR;
  }
  if (!target) {
    target = spn_session_add_target(session, pkg, info);
    sp_assert(target->build);
    spn_try(set_target_kind(session, target));
  }
  *result = target;
  return SPN_OK;
}

static void create_target_objects(spn_session_t* session, spn_target_unit_t* target) {
  spn_pkg_unit_t* pkg = target->pkg;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_str_t) source = collect_target_source(scratch.mem, pkg, target);

  sp_da_for(source, j) {
    sp_str_t relative = source[j];
    sp_str_t file = relative;
    if (sp_fs_is_absolute(relative)) {
      relative = sp_str_strip_left(relative, pkg->paths.recipe);
      relative = sp_str_strip_left(relative, sp_str_lit("/"));
    }
    else {
      file = sp_fs_join_path(session->mem, pkg->paths.source, relative);
    }

    spn_lang_t lang = spn_lang_from_path(relative);

    // Object libs publish their objects as artifacts; everyone else keeps
    // them as intermediates. The object name keeps the full source-relative
    // path, extension included, so colliding sources stay distinct.
    sp_str_t object_dir = target->lib_kind == SPN_LIB_KIND_OBJECT ?
      target->paths.lib :
      sp_fs_join_path(session->mem, target->paths.object, target->info->name);
    sp_str_t object_path = sp_fs_join_path(session->mem, object_dir, sp_fmt(scratch.mem, "{}.o", SP_FMT_STR(relative)).value);
    spn_compile_unit_id_t id = {
      .target = target->id,
      .source = sp_intern_get_or_insert(session->intern, file),
    };

    if (!sp_om_has(session->units.objects, id)) {
      sp_om_insert(session->units.objects, id, ((spn_compile_unit_t) {
        .id = id,
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

    spn_compile_unit_t* object = sp_om_get(session->units.objects, id);
    sp_da_push(target->objects, object);
  }
  sp_mem_end_scratch(scratch);
}

static void add_metaprogram_dep(spn_target_unit_t* program, spn_pkg_unit_t* dep) {
  sp_da_for(program->deps.package, it) {
    if (spn_pkg_id_eq(program->deps.package[it]->id.pkg, dep->id.pkg)) {
      return;
    }
  }
  sp_da_push(program->deps.package, dep);

  sp_da_for(dep->deps, it) {
    if (dep->deps[it].kind != SPN_DEP_KIND_PACKAGE) {
      continue;
    }
    add_metaprogram_dep(program, dep->deps[it].unit);
  }
}

static spn_err_t add_metaprogram_target(spn_session_t* session, spn_pkg_unit_t* unit, spn_target_info_t* info, spn_target_unit_t** result) {
  spn_target_unit_t* target = SP_NULLPTR;
  spn_try(ensure_target(session, unit, info, &target));
  create_target_objects(session, target);
  *result = target;
  return SPN_OK;
}

static void init_program_runtime(spn_pkg_unit_t* unit) {
  spn_pkg_unit_t* program = unit->program;
  sp_assert(program);
  if (program->meta.configure.target) {
    spn_wasm_script_init(
      &unit->wasm.configure,
      get_target_output_path(unit->session->mem, program->meta.configure.target)
    );
  }
  if (program->meta.build.target) {
    spn_wasm_script_init(
      &unit->wasm.build,
      get_target_output_path(unit->session->mem, program->meta.build.target)
    );
  }
}

static spn_err_union_t build_target_invocations(spn_target_unit_t* target) {
  try_union(spn_build_compile_invocations(target));
  return spn_build_link_invocation(target);
}

spn_err_union_t add_program_units(spn_session_t* session) {
  sp_da(spn_pkg_id_t) pending = sp_da_new(session->mem, spn_pkg_id_t);
  sp_ht(spn_pkg_id_t, u8) added = SP_NULLPTR;
  sp_ht_init(session->mem, added);
  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* unit = build->packages[it];
      if (!sp_ht_getp(added, unit->id.pkg)) {
        sp_ht_insert(added, unit->id.pkg, (u8)true);
        sp_da_push(pending, unit->id.pkg);
      }
    }
  }

  sp_for(it, sp_da_size(pending)) {
    spn_pkg_unit_t* unit = add_package_units(
      session,
      session->units.metaprogram,
      pending[it],
      spn_dep_kind_bit(SPN_DEP_KIND_BUILD)
    );
    sp_da_for(unit->deps, it) {
      spn_pkg_id_t id = unit->deps[it].unit->id.pkg;
      if (sp_ht_getp(added, id)) {
        continue;
      }
      sp_ht_insert(added, id, (u8)true);
      sp_da_push(pending, id);
    }
  }

  sp_da_for(session->units.metaprogram->packages, it) {
    spn_pkg_unit_t* unit = session->units.metaprogram->packages[it];
    unit->program = unit;
    if (!sp_da_empty(unit->meta.configure.info->source)) {
      try_as_union(add_metaprogram_target(session, unit, unit->meta.configure.info, &unit->meta.configure.target));
    }
    if (!sp_da_empty(unit->meta.build.info->source)) {
      try_as_union(add_metaprogram_target(session, unit, unit->meta.build.info, &unit->meta.build.target));
    }

    sp_da_for(unit->deps, it) {
      spn_pkg_dep_t* dep = &unit->deps[it];
      if (dep->kind != SPN_DEP_KIND_BUILD) {
        continue;
      }
      if (unit->meta.configure.target) {
        add_metaprogram_dep(unit->meta.configure.target, dep->unit);
      }
      if (unit->meta.build.target) {
        add_metaprogram_dep(unit->meta.build.target, dep->unit);
      }
    }
  }

  sp_da_for(session->units.metaprogram->packages, it) {
    spn_pkg_unit_t* unit = session->units.metaprogram->packages[it];
    if (unit->meta.configure.target) {
      try_union(build_target_invocations(unit->meta.configure.target));
    }
    if (unit->meta.build.target) {
      try_union(build_target_invocations(unit->meta.build.target));
    }
    init_program_runtime(unit);
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* unit = build->packages[it];
      unit->program = spn_session_find_pkg_unit(session, session->units.metaprogram, unit->id.pkg);
      sp_assert(unit->program);
      init_program_runtime(unit);
    }
  }

  return spn_result(SPN_OK);
}

static bool is_root_target(spn_session_t* session, spn_build_plan_t* plan, spn_target_unit_t* target) {
  sp_da_for(plan->roots, it) {
    if (spn_session_get_target_unit(session, plan->roots[it]) == target) {
      return true;
    }
  }
  return false;
}

static bool target_rule_requests_name(const spn_target_rule_t* rule, sp_str_t name) {
  if (rule->kind != SPN_TARGET_RULE_NAMED) {
    return false;
  }
  sp_da_for(rule->names, it) {
    if (sp_str_equal(rule->names[it], name)) {
      return true;
    }
  }
  return false;
}

static bool target_selection_matches_name(const spn_target_selection_t* selection, const spn_pkg_info_t* pkg, sp_str_t name) {
  return
    (target_rule_requests_name(&selection->targets.lib, name) && sp_str_om_has(pkg->libs, name)) ||
    (target_rule_requests_name(&selection->targets.bin, name) && sp_str_om_has(pkg->exes, name)) ||
    (target_rule_requests_name(&selection->targets.test, name) && sp_str_om_has(pkg->tests, name)) ||
    (target_rule_requests_name(&selection->targets.script, name) && sp_str_om_has(pkg->scripts, name));
}

static spn_err_t validate_target_selection(const spn_target_selection_t* selection, const spn_pkg_info_t* pkg) {
  const spn_target_rule_t* rules [] = {
    &selection->targets.lib,
    &selection->targets.bin,
    &selection->targets.test,
    &selection->targets.script,
  };
  sp_carr_for(rules, rt) {
    const spn_target_rule_t* rule = rules[rt];
    if (rule->kind != SPN_TARGET_RULE_NAMED) {
      continue;
    }
    sp_da_for(rule->names, it) {
      sp_str_t name = rule->names[it];
      if (target_selection_matches_name(selection, pkg, name)) {
        continue;
      }
      spn_log_error("target {.yellow} is not defined for the selected target kinds", SP_FMT_STR(name));
      return SPN_ERROR;
    }
  }
  return SPN_OK;
}

static spn_err_t add_plan_targets(spn_session_t* session, spn_build_plan_t* plan, spn_pkg_unit_t* pkg, spn_target_map_t targets) {
  sp_str_om_for(targets, it) {
    spn_target_info_t* info = sp_str_om_at(targets, it);
    if (!spn_target_selection_pass(&plan->selection, info)) {
      continue;
    }

    bool staged_at_root = info->kind == SPN_TARGET_EXE || info->kind == SPN_TARGET_SCRIPT;
    if (staged_at_root && exe_name_reserved(info->name)) {
      spn_log_error(
        "{.cyan} names an executable {.yellow}, which collides with a build output directory (store, work, test)",
        SP_FMT_STR(pkg->info->name),
        SP_FMT_STR(info->name)
      );
      return SPN_ERROR;
    }

    spn_target_unit_t* target = SP_NULLPTR;
    spn_try(ensure_target(session, pkg, info, &target));
    if (!is_root_target(session, plan, target)) {
      sp_da_push(plan->roots, target->id);
    }
  }
  return SPN_OK;
}

static spn_err_t ensure_sibling_targets(spn_session_t* session, sp_da(spn_target_unit_t*) pending) {
  while (!sp_da_empty(pending)) {
    spn_target_unit_t* unit = *sp_da_back(pending);
    sp_da_pop(pending);
    sp_da_for(unit->info->deps, it) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[it]);
      if (find_dep_unit(session, unit->pkg, qualified)) {
        continue;
      }
      if (spn_session_find_target_in_pkg(session, unit->pkg, unit->info->deps[it])) {
        continue;
      }
      spn_target_info_t* info = spn_pkg_get_target_ex(unit->pkg->info, unit->info->deps[it]);
      if (!info) {
        continue;
      }
      spn_target_unit_t* target = SP_NULLPTR;
      spn_try(ensure_target(session, unit->pkg, info, &target));
      sp_da_push(pending, target);
    }
  }
  return SPN_OK;
}

static sp_da(spn_target_unit_t*) collect_plan_targets(spn_session_t* session) {
  sp_da(spn_target_unit_t*) targets = sp_da_new(session->mem, spn_target_unit_t*);
  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* pkg = build->packages[it];
      sp_da_for(pkg->targets, it) {
        sp_da_push(targets, pkg->targets[it]);
      }
    }
  }
  return targets;
}

spn_task_step_t spn_task_create_units(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_pkg_id_t root = spn_session_root_pkg(session);

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* pkg = build->packages[it];
      if (spn_pkg_id_eq(pkg->id.pkg, root)) {
        continue;
      }
      sp_str_om_for(pkg->info->libs, it) {
        spn_target_unit_t* target = SP_NULLPTR;
        if (ensure_target(session, pkg, sp_str_om_at(pkg->info->libs, it), &target)) {
          return spn_task_fail(SPN_ERROR);
        }
      }
    }
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_plan_t* plan = &session->plan.builds[it];
    spn_pkg_unit_t* pkg = spn_session_find_pkg_unit(session, plan->build, root);
    sp_assert(pkg);
    if (validate_target_selection(&plan->selection, pkg->info) ||
        add_plan_targets(session, plan, pkg, pkg->info->libs) ||
        add_plan_targets(session, plan, pkg, pkg->info->exes) ||
        add_plan_targets(session, plan, pkg, pkg->info->scripts) ||
        add_plan_targets(session, plan, pkg, pkg->info->tests)) {
      return spn_task_fail(SPN_ERROR);
    }
  }

  sp_da(spn_target_unit_t*) pending = collect_plan_targets(session);
  if (ensure_sibling_targets(session, pending)) {
    return spn_task_fail(SPN_ERROR);
  }

  sp_da(spn_target_unit_t*) targets = collect_plan_targets(session);
  sp_da_for(targets, it) {
    spn_target_unit_t* unit = targets[it];
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
        return spn_task_fail(SPN_ERROR);
      }
    }
  }

  sp_da_for(targets, it) {
    spn_target_unit_t* target = targets[it];
    if (target->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }

    create_target_objects(session, target);
  }

  sp_om_for(session->units.objects, it) {
    spn_compile_unit_t* object = sp_om_at(session->units.objects, it);
    spn_toolchain_info_t* toolchain = object->target->build->toolchain->info;
    if (object->lang != SPN_LANG_CXX || spn_toolchain_has_cxx(toolchain)) {
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

  spn_err_union_t err = spn_result(SPN_OK);
  sp_da_for(targets, it) {
    err = build_target_invocations(targets[it]);
    if (err.kind) {
      break;
    }
  }
  if (err.kind) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = err,
    });
    return spn_task_fail(SPN_ERROR);
  }
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));

  return spn_task_done();
}
