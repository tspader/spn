#include "sp.h"
#include "sp/macro.h"
#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "sp/sp_om.h"
#include "session/types.h"
#include "target/types.h"
#include "task/types.h"

#include "compiler/driver.h"
#include "enum/enum.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "filter/filter.h"
#include "intern/intern.h"
#include "log/log.h"
#include "pkg/id.h"
#include "pkg/pkg.h"
#include "session/invocation.h"
#include "session/session.h"
#include "sp/sp_glob.h"
#include "target/mutate.h"
#include "target/select.h"
#include "task/build/build.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
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

static spn_err_union_t set_target_kind(spn_session_t* session, spn_target_unit_t* target) {
  spn_target_info_t* info = target->info;

  switch (info->kind) {
    case SPN_TARGET_EXE:
    case SPN_TARGET_SCRIPT:
    case SPN_TARGET_TEST: {
      target->kind = SPN_CC_OUTPUT_EXE;
      return spn_result(SPN_OK);
    }
    case SPN_TARGET_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_BUILD_METAPROGRAM: {
      target->kind = SPN_CC_OUTPUT_REACTOR;
      return spn_result(SPN_OK);
    }
    case SPN_TARGET_LIB: {
      if (spn_linkage_set_has(info->linkages, SPN_LIB_KIND_OBJECT) || info->no_link) {
        target->lib_kind = spn_linkage_set_default(info->linkages);
      }
      else {
        spn_kind_query_t query = {
          .config = spn_session_config_kind(session, target->pkg->info->name),
          .linkage = target->pkg->build->profile.linkage,
        };

        if (spn_target_select_lib_kind(info, query, &target->lib_kind)) {
          sp_str_t requested = spn_linkage_to_str(query.config.some ? query.config.value : query.linkage);
          sp_str_t requester = query.config.some ? sp_str_lit("the root manifest") : sp_str_lit("the profile");
          // @spader Error message rendering does not belong here
          spn_log_error(
            "{.cyan} doesn't support {.yellow} ({} requested it)",
            SP_FMT_STR(target->pkg->info->name),
            SP_FMT_STR(requested),
            SP_FMT_STR(requester)
          );
          return spn_err_reported(SPN_ERROR);
        }
      }

      switch (target->lib_kind) {
        case SPN_LIB_KIND_STATIC: target->kind = SPN_CC_OUTPUT_STATIC_LIB; break;
        case SPN_LIB_KIND_SHARED: target->kind = SPN_CC_OUTPUT_SHARED_LIB; break;
        case SPN_LIB_KIND_SOURCE:
        case SPN_LIB_KIND_OBJECT: target->kind = SPN_CC_OUTPUT_OBJECT; break;
        case SPN_LIB_KIND_NONE: break;
      }
      return spn_result(SPN_OK);
    }
  }

  SP_UNREACHABLE_RETURN(spn_result(SPN_ERROR));
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

static spn_err_union_t ensure_target(spn_session_t* session, spn_pkg_unit_t* pkg, spn_target_info_t* info, spn_target_unit_t** result) {
  spn_target_unit_t* target = spn_session_find_target_in_pkg(session, pkg, info->name);
  if (target && target->info != info) {
    spn_log_error(
      "{.cyan} declares a target {.yellow}, which collides with another target of the same name",
      SP_FMT_STR(pkg->info->name),
      SP_FMT_STR(info->name)
    );
    return spn_err_reported(SPN_ERROR);
  }
  if (!target) {
    target = spn_session_add_target(session, pkg, info);
    sp_assert(target->pkg->build);
    try_union(set_target_kind(session, target));
  }
  *result = target;
  return spn_result(SPN_OK);
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
      pkg->paths.lib :
      sp_fs_join_path(session->mem, pkg->paths.object, target->info->name);
    sp_str_t object_path = sp_fs_join_path(session->mem, object_dir, sp_fmt(scratch.mem, "{}.o", SP_FMT_STR(relative)).value);
    spn_compile_unit_id_t id = {
      .target = target->id,
      .source = sp_intern_get_or_insert(session->intern, file),
    };

    if (!sp_om_has(session->units.objects, id)) {
      sp_om_insert(session->units.objects, id, ((spn_compile_unit_t) {
        .id = id,
        .target = target,
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

static void init_metaprogram_runtime(spn_pkg_unit_t* unit) {
  spn_pkg_metaprogram_t* metaprogram = &unit->metaprogram;
  if (metaprogram->configure.target) {
    spn_wasm_script_init(
      &unit->wasm.configure,
      get_target_output_path(unit->session->mem, metaprogram->configure.target)
    );
  }
  if (metaprogram->build.target) {
    spn_wasm_script_init(
      &unit->wasm.build,
      get_target_output_path(unit->session->mem, metaprogram->build.target)
    );
  }
}

static bool is_os_version_lt(spn_os_version_t a, spn_os_version_t b) {
  if (a.major != b.major) return a.major < b.major;
  return a.minor < b.minor;
}

static spn_os_version_t max_os_version(spn_os_version_t current, spn_os_version_t candidate) {
  return is_os_version_lt(current, candidate) ? candidate : current;
}

static bool is_any_object_cxx(sp_da(spn_compile_unit_t*) objects) {
  sp_da_for(objects, it) {
    if (objects[it]->lang == SPN_LANG_CXX) {
      return true;
    }
  }
  return false;
}

static bool is_target_dynamic(spn_target_unit_t* target) {
  return target->kind == SPN_CC_OUTPUT_SHARED_LIB || target->kind == SPN_CC_OUTPUT_REACTOR;
}

static sp_str_t static_archive_path(sp_mem_t mem, spn_target_unit_t* lib) {
  spn_profile_info_t* profile = &lib->pkg->build->profile;
  spn_triple_t triple = { profile->arch, profile->os, profile->abi };
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = spn_triple_lib_file_name(s.mem, triple, lib->info->name, SP_OS_LIB_STATIC);
  sp_str_t path = sp_fs_join_path(mem, lib->pkg->paths.lib, file_name);
  sp_mem_end_scratch(s);
  return path;
}

static void push_frameworks(sp_da(sp_str_t)* frameworks, sp_da(sp_str_t) values) {
  sp_da_for(values, it) {
    bool present = false;
    sp_da_for(*frameworks, jt) {
      if (sp_str_equal((*frameworks)[jt], values[it])) {
        present = true;
        break;
      }
    }
    if (!present) {
      sp_da_push(*frameworks, values[it]);
    }
  }
}

static spn_err_union_t build_target_invocations(spn_target_unit_t* target) {
  spn_pkg_unit_t* pkg = target->pkg;
  sp_mem_t mem = pkg->session->mem;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da_init(mem, target->link.lib_dirs);
  sp_da_init(mem, target->link.system_libs);
  sp_da_init(mem, target->link.whole_archives);
  sp_da_init(mem, target->link.private_libs);
  sp_da_init(mem, target->link.frameworks);

  spn_os_version_t min_os = max_os_version(target->info->macos.min_os, pkg->info->macos.min_os);
  spn_lang_t lang = is_any_object_cxx(target->objects) ? SPN_LANG_CXX : SPN_LANG_C;

  push_frameworks(&target->link.frameworks, target->info->macos.frameworks);
  push_frameworks(&target->link.frameworks, pkg->info->macos.frameworks);

  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* lib = target->deps.target[it];
    min_os = max_os_version(min_os, lib->info->macos.min_os);
    if (lib->info->no_link) continue;

    if (lib->lib_kind != SPN_LIB_KIND_SHARED) {
      push_frameworks(&target->link.frameworks, lib->info->macos.frameworks);
    }

    switch (lib->lib_kind) {
      case SPN_LIB_KIND_STATIC: {
        if (is_target_dynamic(target)) {
          sp_da_push(target->link.whole_archives, static_archive_path(mem, lib));
          break;
        }
        sp_da_push(target->link.lib_dirs, lib->pkg->paths.lib);
        sp_da_push(target->link.system_libs, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SHARED: {
        sp_da_push(target->link.lib_dirs, lib->pkg->paths.lib);
        sp_da_push(target->link.system_libs, lib->info->name);
        break;
      }
      case SPN_LIB_KIND_SOURCE:
      case SPN_LIB_KIND_OBJECT:
      case SPN_LIB_KIND_NONE: {
        break;
      }
    }
  }

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(s.mem, target);

  // Packages must precede the system libraries they need
  target->link.libs = spn_closure_link_libs(mem, closure, pkg);
  sp_da_for(target->link.libs, it) {
    spn_pkg_unit_t* dep = target->link.libs[it].pkg;
    spn_target_unit_t* lib = target->link.libs[it].lib;

    if (lang != SPN_LANG_CXX && lib->lib_kind == SPN_LIB_KIND_STATIC && is_any_object_cxx(lib->objects)) {
      lang = SPN_LANG_CXX;
    }

    sp_da_push(target->link.lib_dirs, dep->paths.lib);

    if (lib->lib_kind == SPN_LIB_KIND_SHARED) {
      sp_da_push(target->link.system_libs, lib->info->name);
      continue;
    }

    if (!is_target_dynamic(target)) {
      sp_da_push(target->link.system_libs, lib->info->name);
    }
    else if (target->link.libs[it].private) {
      sp_da_push(target->link.private_libs, lib->info->name);
    }
    else {
      sp_da_push(target->link.whole_archives, static_archive_path(mem, lib));
    }
  }

  sp_da_for(pkg->info->system_deps, it) {
    sp_da_push(target->link.system_libs, pkg->info->system_deps[it]);
  }
  sp_da_for(closure, it) {
    spn_pkg_unit_t* dep = closure[it].pkg;
    if (!dep || dep == pkg) continue;

    min_os = max_os_version(min_os, dep->info->macos.min_os);

    sp_da_for(dep->info->system_deps, st) {
      sp_da_push(target->link.system_libs, dep->info->system_deps[st]);
    }
    bool static_link = false;
    sp_da_for(dep->libs, lt) {
      spn_target_unit_t* lib = dep->libs[lt];
      min_os = max_os_version(min_os, lib->info->macos.min_os);
      if (lib->info->no_link) continue;
      if (lib->lib_kind == SPN_LIB_KIND_SHARED) continue;
      static_link = true;
      push_frameworks(&target->link.frameworks, lib->info->macos.frameworks);
    }
    if (static_link || sp_da_empty(dep->libs)) {
      push_frameworks(&target->link.frameworks, dep->info->macos.frameworks);
    }
  }

  target->link.min_os = min_os;
  target->link.lang = lang;

  sp_mem_end_scratch(s);

  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &target->pkg->build->toolchain->cc;

  if (!sp_da_empty(target->objects)) {
    try_union(spn_cc_validate_compile(toolchain, profile));
  }

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      return spn_cc_validate_archive(toolchain, profile);
    }
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      try_union(spn_cc_validate_archive(toolchain, profile));
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.frameworks));
    }
    case SPN_CC_OUTPUT_EXE: {
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.frameworks));
    }
    case SPN_CC_OUTPUT_OBJECT: {
      return spn_result(SPN_OK);
    }
  }

  sp_unreachable_return(spn_result(SPN_ERROR));
}

static void collect_unit_targets(sp_da(spn_target_unit_t*)* targets, sp_da(spn_pkg_unit_t*) units) {
  sp_da_for(units, it) {
    spn_pkg_unit_t* pkg = units[it];
    sp_da_for(pkg->targets, jt) {
      sp_da_push(*targets, pkg->targets[jt]);
    }
  }
}

static spn_err_union_t ensure_sibling_targets(spn_session_t* session, sp_da(spn_target_unit_t*)* targets) {
  sp_for(it, sp_da_size(*targets)) {
    spn_target_unit_t* unit = (*targets)[it];
    sp_da_for(unit->info->deps, jt) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[jt]);
      if (find_dep_unit(session, unit->pkg, qualified)) {
        continue;
      }
      if (spn_session_find_target_in_pkg(session, unit->pkg, unit->info->deps[jt])) {
        continue;
      }
      spn_target_info_t* info = spn_pkg_get_target_ex(unit->pkg->info, unit->info->deps[jt]);
      if (!info) {
        continue;
      }
      spn_target_unit_t* target = SP_NULLPTR;
      try_union(ensure_target(session, unit->pkg, info, &target));
      sp_da_push(*targets, target);
    }
  }
  return spn_result(SPN_OK);
}

static spn_err_union_t resolve_target_deps(spn_session_t* session, sp_da(spn_target_unit_t*) targets) {
  sp_da_for(targets, it) {
    spn_target_unit_t* unit = targets[it];
    sp_da_for(unit->info->deps, jt) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[jt]);
      struct {
        spn_pkg_unit_t* pkg;
        spn_target_unit_t* target;
      } candidates = {
        .pkg = find_dep_unit(session, unit->pkg, qualified),
        .target = spn_session_find_target_in_pkg(session, unit->pkg, unit->info->deps[jt])
      };

      if (candidates.pkg) {
        sp_da_push(unit->deps.package, candidates.pkg);
      }
      else if (candidates.target) {
        sp_da_push(unit->deps.target, candidates.target);
      }
      else {
        spn_log_error("failed to find {.cyan} as a package or target", SP_FMT_STR(unit->info->deps[jt]));
        return spn_err_reported(SPN_ERROR);
      }
    }
  }
  return spn_result(SPN_OK);
}

spn_err_union_t add_metaprogram_units(spn_session_t* session) {
  spn_build_unit_t* world = session->units.metaprogram;

  sp_da(spn_pkg_unit_t*) units = sp_da_new(session->mem, spn_pkg_unit_t*);
  sp_da_for(world->hosts, it) {
    sp_da_push(units, world->hosts[it]);
  }
  sp_da_for(world->packages, it) {
    sp_da_push(units, world->packages[it]);
  }

  sp_da_for(units, it) {
    spn_pkg_unit_t* unit = units[it];
    if (!sp_da_empty(unit->metaprogram.configure.info->source)) {
      try_union(ensure_target(session, unit, unit->metaprogram.configure.info, &unit->metaprogram.configure.target));
    }
    if (!sp_da_empty(unit->metaprogram.build.info->source)) {
      try_union(ensure_target(session, unit, unit->metaprogram.build.info, &unit->metaprogram.build.target));
    }
  }

  sp_da_for(world->packages, it) {
    spn_pkg_unit_t* pkg = world->packages[it];
    sp_str_om_for(pkg->info->libs, jt) {
      spn_target_unit_t* target = SP_NULLPTR;
      try_union(ensure_target(session, pkg, sp_str_om_at(pkg->info->libs, jt), &target));
    }
  }

  sp_da(spn_target_unit_t*) targets = sp_da_new(session->mem, spn_target_unit_t*);
  collect_unit_targets(&targets, units);
  try_union(ensure_sibling_targets(session, &targets));
  try_union(resolve_target_deps(session, targets));

  sp_da_for(targets, it) {
    if (targets[it]->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }
    create_target_objects(session, targets[it]);
  }
  sp_da_for(units, it) {
    spn_target_unit_t* configure = units[it]->metaprogram.configure.target;
    if (configure) {
      create_target_objects(session, configure);
      sp_assert(!sp_da_empty(configure->objects));
    }
  }

  sp_da_for(targets, it) {
    try_union(build_target_invocations(targets[it]));
  }
  sp_da_for(units, it) {
    spn_pkg_unit_t* unit = units[it];
    if (unit->metaprogram.configure.target) {
      try_union(build_target_invocations(unit->metaprogram.configure.target));
    }
    init_metaprogram_runtime(unit);
  }

  sp_da_for(units, it) {
    spn_pkg_unit_t* owner = units[it];
    if (!owner->metaprogram.configure.target && !owner->metaprogram.build.target) {
      continue;
    }
    sp_da_for(session->plan.builds, jt) {
      spn_pkg_unit_t* unit = spn_session_find_pkg_unit(session, session->plan.builds[jt].build, owner->id.pkg);
      if (!unit) {
        continue;
      }
      unit->metaprogram = owner->metaprogram;
      init_metaprogram_runtime(unit);
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

static spn_err_union_t validate_target_selection(const spn_target_selection_t* selection, const spn_pkg_info_t* pkg) {
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
      return spn_err_reported(SPN_ERROR);
    }
  }
  return spn_result(SPN_OK);
}

static spn_err_union_t add_plan_targets(spn_session_t* session, spn_build_plan_t* plan, spn_pkg_unit_t* pkg, spn_target_map_t targets) {
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
      return spn_err_reported(SPN_ERROR);
    }

    spn_target_unit_t* target = SP_NULLPTR;
    try_union(ensure_target(session, pkg, info, &target));
    if (!is_root_target(session, plan, target)) {
      sp_da_push(plan->roots, target->id);
    }
  }
  return spn_result(SPN_OK);
}

spn_task_step_t spn_task_create_units(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_pkg_id_t root = spn_session_root_pkg(session);

  sp_da_for(session->plan.builds, it) {
    spn_build_plan_t* plan = &session->plan.builds[it];
    sp_da_for(plan->build->packages, jt) {
      spn_pkg_unit_t* pkg = plan->build->packages[jt];
      if (spn_pkg_id_eq(pkg->id.pkg, root)) {
        continue;
      }
      sp_str_om_for(pkg->info->libs, kt) {
        spn_target_unit_t* target = SP_NULLPTR;
        spn_try_step(ensure_target(session, pkg, sp_str_om_at(pkg->info->libs, kt), &target));
      }
    }

    spn_pkg_unit_t* pkg = spn_session_find_pkg_unit(session, plan->build, root);
    sp_assert(pkg);
    spn_try_step(validate_target_selection(&plan->selection, pkg->info));
    spn_try_step(add_plan_targets(session, plan, pkg, pkg->info->libs));
    spn_try_step(add_plan_targets(session, plan, pkg, pkg->info->exes));
    spn_try_step(add_plan_targets(session, plan, pkg, pkg->info->scripts));
    spn_try_step(add_plan_targets(session, plan, pkg, pkg->info->tests));
  }

  sp_da(spn_target_unit_t*) targets = sp_da_new(session->mem, spn_target_unit_t*);
  sp_da_for(session->plan.builds, it) {
    collect_unit_targets(&targets, session->plan.builds[it].build->packages);
  }
  spn_try_step(ensure_sibling_targets(session, &targets));
  spn_try_step(resolve_target_deps(session, targets));

  sp_da_for(targets, it) {
    spn_target_unit_t* target = targets[it];
    if (target->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }

    create_target_objects(session, target);
  }

  sp_om_for(session->units.objects, it) {
    spn_compile_unit_t* object = sp_om_at(session->units.objects, it);
    spn_toolchain_info_t* toolchain = object->target->pkg->build->toolchain->info;
    if (object->lang != SPN_LANG_CXX || spn_toolchain_has_cxx(toolchain)) {
      continue;
    }

    return spn_task_fail(SPN_ERR_TOOLCHAIN_NO_CXX, .toolchain = { .name = toolchain->name });
  }

  sp_da_for(targets, it) {
    spn_try_step(build_target_invocations(targets[it]));
  }
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));

  return spn_task_done();
}
