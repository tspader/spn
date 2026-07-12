#include "sp.h"
#include "sp/macro.h"
#include "app/types.h"
#include "forward/types.h"
#include "intern/intern.h"
#include "log/lazy/lazy.h"
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
    case SPN_TARGET_MODULE: {
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
    sp_da_push(pkg->objects, object);
    sp_da_push(target->objects, object);
  }
  sp_mem_end_scratch(scratch);
}

static void add_module_dep(spn_target_unit_t* module, spn_pkg_unit_t* dep) {
  sp_da_for(module->deps.package, it) {
    if (spn_pkg_id_eq(module->deps.package[it]->id.pkg, dep->id.pkg)) {
      return;
    }
  }
  sp_da_push(module->deps.package, dep);

  // Mirror resolve's scope flattening: a dep's package deps re-export to its
  // consumers unless the provider is shared, and wasm providers never are.
  // Script-ctx deps hold PACKAGE edges only, so no kind filter
  sp_da_for(dep->deps, it) {
    add_module_dep(module, dep->deps[it].unit);
  }
}

static spn_err_t add_script_target(spn_session_t* session, spn_pkg_unit_t* unit, spn_target_info_t* info, spn_target_unit_t** result) {
  spn_target_unit_t* target = SP_NULLPTR;
  spn_try(ensure_target(session, unit, info, &target));

  // Modules compile in the script ctx regardless of which unit parents them.
  // When the parent lives in another ctx, the module's outputs move under the
  // script profile so ctx artifacts never mix.
  if (target->build != session->plan.script) {
    target->build = session->plan.script;
    sp_str_t work = sp_fs_join_path(session->mem, session->plan.script->paths.profile, sp_str_lit("work"));
    target->paths.work = sp_fs_join_path(session->mem, work, unit->info->name);
    target->paths.generated = sp_fs_join_path(session->mem, target->paths.work, SP_LIT("spn"));
    target->paths.object = sp_fs_join_path(session->mem, target->paths.generated, sp_str_lit("object"));
    target->paths.logs.build = sp_fs_join_path(session->mem, target->paths.work, sp_fmt(session->mem, "{}.build.log", SP_FMT_STR(info->name)).value);
    target->paths.logs.test = sp_fs_join_path(session->mem, target->paths.work, sp_fmt(session->mem, "{}.test.log", SP_FMT_STR(info->name)).value);
    target->paths.logs.jsonl = sp_fs_join_path(session->mem, target->paths.work, sp_fmt(session->mem, "{}.build.jsonl", SP_FMT_STR(info->name)).value);
    sp_fs_create_dir(target->paths.work);
    sp_fs_create_dir(target->paths.generated);
    sp_fs_create_dir(target->paths.object);
    spn_lazy_log_init(&target->logs.build, target->paths.logs.build);
    spn_lazy_log_init(&target->logs.jsonl, target->paths.logs.jsonl);
  }

  create_target_objects(session, target);
  *result = target;
  return SPN_OK;
}

static bool scripted_push(sp_da(spn_pkg_unit_t*)* scripted, spn_pkg_unit_t* unit) {
  if (sp_da_empty(unit->script.configure.source) && sp_da_empty(unit->script.build.source)) {
    return false;
  }
  sp_da_for(*scripted, st) {
    if (spn_pkg_id_eq((*scripted)[st]->id.pkg, unit->id.pkg)) {
      return false;
    }
  }
  sp_da_push(*scripted, unit);
  return true;
}

spn_err_union_t add_script_units(spn_session_t* session) {
  sp_assert(session->units.script);

  spn_build_unit_t* build = sp_alloc_type(session->mem, spn_build_unit_t);
  *build = (spn_build_unit_t) {
    .id = (spn_build_unit_id_t)sp_da_size(session->plan.builds),
    .profile = {
      .name = sp_str_lit("script"),
      .arch = SPN_ARCH_WASM32,
      .os = SPN_OS_WASI,
      .abi = SPN_ABI_NONE,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_2,
      .standard = SPN_C99,
      .linkage = SPN_LIB_KIND_STATIC,
    },
    .toolchain = session->units.script,
    .visibility = SPN_SYMBOL_VISIBILITY_HIDDEN,
    .dep_kinds = spn_dep_kind_bit(SPN_DEP_KIND_BUILD),
    .paths = {
      .profile = sp_fs_join_path(session->mem, session->paths.build, sp_str_lit("script"))
    },
  };
  sp_da_init(session->mem, build->include);
  sp_da_push(build->include, spn.paths.include);

  session->plan.script = build;


  sp_da(spn_pkg_unit_t*) scripted = sp_da_new(session->mem, spn_pkg_unit_t*);
  sp_assert(!session->plan.script);
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    if (!sp_da_empty(unit->info->configure.source)) {
      spn_wasm_script_init(&unit->wasm.configure, sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("configure.wasm")));
    }
    spn_wasm_script_init(&unit->wasm.build, sp_fs_join_path(session->mem, unit->paths.generated, SP_LIT("build.wasm")));

  }



  // The script ctx holds only the BUILD dep closures of scripted packages;
  // scripted packages themselves stay in their own ctx and parent the module
  // targets. A BUILD dep can itself be scripted, so the worklist grows.
  for (u32 it = 0; it < sp_da_size(scripted); it++) {
    spn_resolved_pkg_t* pkg = sp_ht_getp(session->resolve, scripted[it]->id.pkg);
    sp_assert(pkg);
    sp_da_for(pkg->edges, et) {
      if (pkg->edges[et].kind != SPN_DEP_KIND_BUILD) {
        continue;
      }
      spn_pkg_unit_t* dep = add_package_units(session, build, pkg->edges[et].id, spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE));
      scripted_push(&scripted, dep);
    }
  }

  sp_da_for(scripted, it) {
    spn_pkg_unit_t* unit = scripted[it];

    struct {
      spn_target_unit_t* build;
      spn_target_unit_t* configure;
    } scripts = sp_zero;
    if (!sp_da_empty(unit->script.configure.source)) {
      try_as_union(add_script_target(session, unit, &unit->script.configure, &scripts.configure));
    }
    if (!sp_da_empty(unit->script.build.source)) {
      try_as_union(add_script_target(session, unit, &unit->script.build, &scripts.build));
    }

    spn_resolved_pkg_t* resolved = sp_ht_getp(session->resolve, unit->id.pkg);
    sp_assert(resolved);
    sp_da_for(resolved->edges, et) {
      if (resolved->edges[et].kind != SPN_DEP_KIND_BUILD) {
        continue;
      }
      spn_pkg_unit_t* dep = spn_session_find_pkg_unit(session, build, resolved->edges[et].id);
      sp_assert(dep);
      if (scripts.configure) {
        add_module_dep(scripts.configure, dep);
      }
      if (scripts.build) {
        add_module_dep(scripts.build, dep);
      }
    }

    sp_om_for(session->units.packages, j) {
      spn_pkg_unit_t* candidate = sp_om_at(session->units.packages, j);
      if (!spn_pkg_id_eq(candidate->id.pkg, unit->id.pkg)) {
        continue;
      }
      if (scripts.configure) {
        spn_wasm_script_init(&candidate->wasm.configure, true, get_target_output_path(session->mem, scripts.configure));
      }
      if (scripts.build) {
        spn_wasm_script_init(&candidate->wasm.build, true, get_target_output_path(session->mem, scripts.build));
      }
    }
  }

  // Script-ctx units exist only to serve modules: each must be reachable from
  // some module target's deps. One that isn't is a container regression.
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ht(spn_pkg_id_t, u8) reachable = SP_NULLPTR;
  sp_ht_init(scratch.mem, reachable);
  sp_da(spn_pkg_unit_t*) frontier = sp_da_new(scratch.mem, spn_pkg_unit_t*);
  sp_da_for(scripted, it) {
    const c8* names [] = { "configure", "build" };
    sp_carr_for(names, nt) {
      spn_target_unit_t* module = spn_session_find_target_in_pkg(session, scripted[it], sp_str_view(names[nt]));
      if (!module || module->info->kind != SPN_TARGET_MODULE) {
        continue;
      }
      sp_da_for(module->deps.package, dt) {
        sp_da_push(frontier, module->deps.package[dt]);
      }
    }
  }
  while (!sp_da_empty(frontier)) {
    spn_pkg_unit_t* unit = *sp_da_back(frontier);
    sp_da_pop(frontier);
    if (sp_ht_getp(reachable, unit->id.pkg)) {
      continue;
    }
    sp_ht_insert(reachable, unit->id.pkg, (u8)true);
    sp_da_for(unit->deps, dt) {
      sp_da_push(frontier, unit->deps[dt].unit);
    }
  }
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    if (unit->build != build) {
      continue;
    }
    sp_assert(sp_ht_getp(reachable, unit->id.pkg));
  }
  sp_mem_end_scratch(scratch);

  try_union(spn_session_build_invocations(session));
  return spn_build_link_invocations(session);
}

static bool is_root_target(spn_session_t* session, spn_build_plan_t* plan, spn_target_unit_t* target) {
  sp_da_for(plan->roots, it) {
    if (spn_session_get_target_unit(session, plan->roots[it]) == target) {
      return true;
    }
  }
  return false;
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

spn_task_step_t spn_task_create_units(spn_app_t* app) {
  spn_session_t* session = &app->session;

  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* pkg = sp_om_at(session->units.packages, it);
    sp_str_om_for(pkg->info->libs, it) {
      spn_target_unit_t* target = SP_NULLPTR;
      if (ensure_target(session, pkg, sp_str_om_at(pkg->info->libs, it), &target)) {
        return spn_task_fail(SPN_ERROR);
      }
    }
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_plan_t* plan = &session->plan.builds[it];
    sp_da_for(session->plan.requested, rt) {
      spn_pkg_unit_t* pkg = spn_session_find_pkg_unit(session, plan->build, session->plan.requested[rt]);
      sp_assert(pkg);
      if (add_plan_targets(session, plan, pkg, pkg->info->libs) ||
          add_plan_targets(session, plan, pkg, pkg->info->exes) ||
          add_plan_targets(session, plan, pkg, pkg->info->scripts) ||
          add_plan_targets(session, plan, pkg, pkg->info->tests)) {
        return spn_task_fail(SPN_ERROR);
      }
    }
  }

  sp_da(spn_target_unit_t*) pending = sp_da_new(session->mem, spn_target_unit_t*);
  sp_om_for(session->units.targets, it) {
    sp_da_push(pending, sp_om_at(session->units.targets, it));
  }
  if (ensure_sibling_targets(session, pending)) {
    return spn_task_fail(SPN_ERROR);
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
  // to be compiled. Module targets already did this at sync.
  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* target = sp_om_at(session->units.targets, it);

    if (target->info->kind == SPN_TARGET_MODULE) continue;

    // Source libs are consumed as source; we never compile them ourselves
    if (target->lib_kind == SPN_LIB_KIND_SOURCE) continue;

    create_target_objects(session, target);
  }

  sp_om_for(session->units.objects, it) {
    spn_compile_unit_t* object = sp_om_at(session->units.objects, it);
    spn_toolchain_t* toolchain = object->target->build->toolchain->toolchain;
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

  spn_err_union_t err = spn_session_build_invocations(session);
  if (!err.kind) {
    err = spn_build_link_invocations(session);
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
