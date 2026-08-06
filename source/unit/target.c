#include "unit/unit.h"

#include "sp.h"
#include "sp/macro.h"
#include "sp/sp_glob.h"
#include "sp/str.h"

#include "ctx/types.h"

#include "compiler/driver.h"
#include "enum/enum.h"
#include "external/wasm/wasm.h"
#include "filter/filter.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "target/mutate.h"
#include "log/lazy/lazy.h"
#include "pkg/pkg.h"
#include "session/invocation.h"
#include "session/session.h"
#include "target/select.h"
#include "op/build/build.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

static spn_target_unit_t* add_target(spn_session_t* s, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  spn_target_unit_id_t id = {
    .pkg = pkg->id,
    .target = sp_intern_get_or_insert(s->ctx->intern, info->name),
  };

  sp_om_insert(s->units.targets, id, SP_ZERO_STRUCT(spn_target_unit_t));
  spn_target_unit_t* target = sp_om_back(s->units.targets);
  target->id = id;
  target->pkg = pkg;
  target->info = info;
  sp_da_init(s->mem, target->objects);
  sp_da_init(s->mem, target->deps);

  switch (info->kind) {
    case SPN_TARGET_LIB: {
      sp_da_push(pkg->targets, target);
      sp_da_push(pkg->libs, target);
      break;
    }
    case SPN_TARGET_EXE:
    case SPN_TARGET_SCRIPT:
    case SPN_TARGET_TEST: {
      sp_da_push(pkg->targets, target);
      break;
    }
    case SPN_TARGET_CONFIGURE_METAPROGRAM: {
      sp_assert(pkg->build == s->units.metaprogram);
      pkg->scripts.configure = target;
      break;
    }
    case SPN_TARGET_BUILD_METAPROGRAM: {
      sp_assert(pkg->build == s->units.metaprogram);
      pkg->scripts.build = target;
      sp_da_push(pkg->targets, target);
      break;
    }
  }

  sp_str_t build_log = sp_fs_join_path(s->mem, pkg->paths.work, sp_fmt(s->mem, "{}.build.log", SP_FMT_STR(info->name)).value);
  sp_str_t jsonl_log = sp_fs_join_path(s->mem, pkg->paths.work, sp_fmt(s->mem, "{}.build.jsonl", SP_FMT_STR(info->name)).value);
  spn_lazy_log_init(&target->logs.build, build_log);
  spn_lazy_log_init(&target->logs.jsonl, jsonl_log);
  return target;
}

static spn_err_union_t set_target_kind(spn_session_t* s, spn_target_unit_t* target) {
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
          .config = spn_session_config_kind(s, target->pkg->info->name),
          .linkage = target->pkg->build->profile.linkage,
        };

        if (spn_target_select_lib_kind(info, query, &target->lib_kind)) {
          return (spn_err_union_t) {
            .kind = SPN_ERR_TARGET_LINKAGE,
            .target = {
              .pkg = target->pkg->info->name,
              .requested = spn_linkage_to_str(query.config.some ? query.config.value : query.linkage),
              .requester = query.config.some ? sp_str_lit("the root manifest") : sp_str_lit("the profile"),
            },
          };
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

static spn_err_union_t ensure_target(spn_session_t* s, spn_pkg_unit_t* pkg, spn_target_info_t* info, spn_target_unit_t** result) {
  spn_target_unit_t* target = spn_session_find_target_in_pkg(s, pkg, info->name);
  if (target && target->info != info) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_TARGET_DUPLICATE,
      .target = {
        .pkg = pkg->info->name,
        .name = info->name,
      },
    };
  }
  if (!target) {
    target = add_target(s, pkg, info);
    spn_try_union(set_target_kind(s, target));
  }
  if (result) *result = target;
  return spn_result(SPN_OK);
}

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

static void create_target_objects(spn_session_t* s, spn_target_unit_t* target) {
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
      file = sp_fs_join_path(s->mem, pkg->paths.source, relative);
    }

    spn_lang_t lang = spn_lang_from_path(relative);

    // Object libs publish their objects as artifacts; everyone else keeps
    // them as intermediates. The object name keeps the full source-relative
    // path, extension included, so colliding sources stay distinct.
    sp_str_t object_dir = target->lib_kind == SPN_LIB_KIND_OBJECT ?
      pkg->paths.lib :
      sp_fs_join_path(s->mem, pkg->paths.object, target->info->name);
    sp_str_t object_path = sp_fs_join_path(s->mem, object_dir, sp_fmt(scratch.mem, "{}.o", SP_FMT_STR(relative)).value);
    spn_compile_unit_id_t id = {
      .target = target->id,
      .source = sp_intern_get_or_insert(s->ctx->intern, file),
    };

    if (!sp_om_has(s->units.objects, id)) {
      sp_om_insert(s->units.objects, id, ((spn_compile_unit_t) {
        .id = id,
        .target = target,
        .lang = lang,
        .paths = {
          .object = object_path,
          .file = file,
        },
      }));
    }

    spn_compile_unit_t* object = sp_om_get(s->units.objects, id);
    sp_da_push(target->objects, object);
  }
  sp_mem_end_scratch(scratch);
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

typedef sp_str_ht(u8) link_framework_set_t;

static void push_frameworks(link_framework_set_t* seen, sp_da(sp_str_t)* frameworks, sp_da(sp_str_t) values) {
  sp_da_for(values, it) {
    if (sp_str_ht_exists(*seen, values[it])) {
      continue;
    }
    sp_str_ht_insert(*seen, values[it], (u8)true);
    sp_da_push(*frameworks, values[it]);
  }
}

static spn_err_union_t render_compile_bases(sp_mem_t mem, spn_target_unit_t* target) {
  sp_da_for(target->objects, it) {
    spn_compile_unit_t* unit = target->objects[it];
    spn_try_union(spn_build_render_compile(mem, unit, &unit->invocation));
  }
  return spn_result(SPN_OK);
}

typedef enum {
  LINK_PLACE_NONE,
  LINK_PLACE_LIB,
  LINK_PLACE_WHOLE_ARCHIVE,
  LINK_PLACE_PRIVATE_LIB,
} link_placement_t;

static link_placement_t link_plan_placement(spn_target_unit_t* target, spn_target_unit_t* lib, bool private) {
  if (lib->info->no_link) {
    return LINK_PLACE_NONE;
  }
  switch (lib->lib_kind) {
    case SPN_LIB_KIND_SHARED: {
      return LINK_PLACE_LIB;
    }
    case SPN_LIB_KIND_STATIC: {
      if (!is_target_dynamic(target)) {
        return LINK_PLACE_LIB;
      }
      if (private) {
        return LINK_PLACE_PRIVATE_LIB;
      }
      return LINK_PLACE_WHOLE_ARCHIVE;
    }
    case SPN_LIB_KIND_SOURCE:
    case SPN_LIB_KIND_OBJECT:
    case SPN_LIB_KIND_NONE: {
      return LINK_PLACE_NONE;
    }
  }
  sp_unreachable_return(LINK_PLACE_NONE);
}

static spn_os_version_t link_plan_min_os(spn_target_unit_t* target, sp_da(spn_closure_entry_t) closure) {
  spn_os_version_t min_os = target->info->macos.min_os;
  sp_da_for(closure, it) {
    min_os = max_os_version(min_os, closure[it].pkg->info->macos.min_os);
    sp_da_for(closure[it].targets, lt) {
      min_os = max_os_version(min_os, closure[it].targets[lt]->info->macos.min_os);
    }
  }
  return min_os;
}

static spn_lang_t link_plan_lang(spn_target_unit_t* target, sp_da(spn_link_lib_t) libs) {
  if (is_any_object_cxx(target->objects)) {
    return SPN_LANG_CXX;
  }
  sp_da_for(libs, it) {
    spn_target_unit_t* lib = libs[it].lib;
    if (lib->lib_kind == SPN_LIB_KIND_STATIC && is_any_object_cxx(lib->objects)) {
      return SPN_LANG_CXX;
    }
  }
  return SPN_LANG_C;
}

static void link_plan_frameworks(spn_target_unit_t* target, sp_da(spn_closure_entry_t) closure, sp_da(sp_str_t)* frameworks) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  link_framework_set_t seen;
  sp_str_ht_init(s.mem, seen);

  push_frameworks(&seen, frameworks, target->info->macos.frameworks);

  sp_da_for(closure, it) {
    spn_closure_entry_t* entry = &closure[it];
    if (entry->links_code) {
      push_frameworks(&seen, frameworks, entry->pkg->info->macos.frameworks);
    }
    sp_da_for(entry->targets, lt) {
      spn_target_unit_t* lib = entry->targets[lt];
      if (lib->info->no_link) continue;
      if (lib->lib_kind == SPN_LIB_KIND_SHARED) continue;
      push_frameworks(&seen, frameworks, lib->info->macos.frameworks);
    }
  }
  sp_mem_end_scratch(s);
}

static spn_link_plan_t link_plan(spn_target_unit_t* target) {
  spn_pkg_unit_t* pkg = target->pkg;
  sp_mem_t mem = pkg->session->mem;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(s.mem, target);
  sp_assert(closure[0].pkg == pkg);

  spn_link_plan_t plan = {
    .libs = spn_closure_get_linked_libs(mem, closure),
    .cc = {
      .kind = target->kind,
      .min_os = link_plan_min_os(target, closure),
      .subsystem = target->info->windows.subsystem,
      .rpath = true,
    },
  };
  plan.cc.lang = link_plan_lang(target, plan.libs);
  sp_da_init(mem, plan.cc.libs);
  sp_da_init(mem, plan.cc.lib_dirs);
  sp_da_init(mem, plan.cc.system_libs);
  sp_da_init(mem, plan.cc.whole_archives);
  sp_da_init(mem, plan.cc.private_libs);
  sp_da_init(mem, plan.cc.frameworks);

  link_plan_frameworks(target, closure, &plan.cc.frameworks);

  sp_da_for(plan.libs, it) {
    spn_link_lib_t* lib = &plan.libs[it];
    switch (link_plan_placement(target, lib->lib, lib->private)) {
      case LINK_PLACE_NONE: {
        break;
      }
      case LINK_PLACE_LIB: {
        sp_da_push(plan.cc.lib_dirs, lib->lib->pkg->paths.lib);
        sp_da_push(plan.cc.libs, lib->lib->info->name);
        break;
      }
      case LINK_PLACE_PRIVATE_LIB: {
        sp_da_push(plan.cc.lib_dirs, lib->lib->pkg->paths.lib);
        sp_da_push(plan.cc.private_libs, lib->lib->info->name);
        break;
      }
      case LINK_PLACE_WHOLE_ARCHIVE: {
        sp_da_push(plan.cc.whole_archives, static_archive_path(mem, lib->lib));
        break;
      }
    }
  }

  sp_da_for(closure, it) {
    sp_da_for(closure[it].pkg->info->system_deps, st) {
      sp_da_push(plan.cc.system_libs, closure[it].pkg->info->system_deps[st]);
    }
  }

  sp_da_for(plan.cc.whole_archives, it) {
    sp_assert(sp_fs_is_absolute(plan.cc.whole_archives[it]));
  }

  sp_mem_end_scratch(s);
  return plan;
}

static spn_err_union_t build_target_plan(spn_target_unit_t* target) {
  spn_pkg_unit_t* pkg = target->pkg;
  spn_profile_info_t* profile = &pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &pkg->build->toolchain->cc;

  target->link = link_plan(target);
  spn_try_union(render_compile_bases(pkg->session->mem, target));

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      return spn_cc_validate_archive(toolchain, profile);
    }
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      spn_try_union(spn_cc_validate_archive(toolchain, profile));
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.cc.frameworks));
    }
    case SPN_CC_OUTPUT_EXE: {
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.cc.frameworks));
    }
    case SPN_CC_OUTPUT_OBJECT: {
      return spn_result(SPN_OK);
    }
  }

  sp_unreachable_return(spn_result(SPN_ERROR));
}

static spn_pkg_unit_t* find_dep_unit(spn_session_t* s, spn_pkg_unit_t* pkg, sp_str_t qualified) {
  const spn_dep_kind_t kinds [] = { SPN_DEP_KIND_PACKAGE, SPN_DEP_KIND_TEST, SPN_DEP_KIND_BUILD };
  sp_carr_for(kinds, it) {
    spn_pkg_unit_t* unit = spn_session_find_dep(s, pkg, qualified, kinds[it]);
    if (unit) {
      return unit;
    }
  }
  return SP_NULLPTR;
}

static void collect_unit_targets(sp_da(spn_target_unit_t*)* targets, sp_da(spn_pkg_unit_t*) units) {
  sp_da_for(units, it) {
    spn_pkg_unit_t* pkg = units[it];
    sp_da_for(pkg->targets, jt) {
      sp_da_push(*targets, pkg->targets[jt]);
    }
  }
}

static spn_err_union_t ensure_sibling_targets(spn_session_t* s, sp_da(spn_target_unit_t*)* targets) {
  sp_for(it, sp_da_size(*targets)) {
    spn_target_unit_t* unit = (*targets)[it];
    sp_da_for(unit->info->deps, jt) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[jt]);
      if (find_dep_unit(s, unit->pkg, qualified)) {
        continue;
      }
      if (spn_session_find_target_in_pkg(s, unit->pkg, unit->info->deps[jt])) {
        continue;
      }
      spn_target_info_t* info = spn_pkg_get_target_ex(unit->pkg->info, unit->info->deps[jt]);
      if (!info) {
        continue;
      }
      spn_target_unit_t* target = SP_NULLPTR;
      spn_try_union(ensure_target(s, unit->pkg, info, &target));
      sp_da_push(*targets, target);
    }
  }
  return spn_result(SPN_OK);
}

static spn_err_union_t resolve_target_deps(spn_session_t* s, sp_da(spn_target_unit_t*) targets) {
  sp_da_for(targets, it) {
    spn_target_unit_t* unit = targets[it];
    sp_da_for(unit->info->deps, jt) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[jt]);
      if (find_dep_unit(s, unit->pkg, qualified)) {
        continue;
      }

      spn_target_unit_t* target = spn_session_find_target_in_pkg(s, unit->pkg, unit->info->deps[jt]);
      if (!target) {
        return (spn_err_union_t) {
          .kind = SPN_ERR_TARGET_DEP,
          .target = { .name = unit->info->deps[jt] },
        };
      }
      sp_da_push(unit->deps, target);
    }
  }
  return spn_result(SPN_OK);
}

static void init_wasm_scripts(spn_session_t* s) {
  sp_om_for(s->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(s->units.packages, it);
    if (!unit->metaprogram) {
      continue;
    }
    if (unit->metaprogram->scripts.configure) {
      spn_wasm_script_init(&unit->wasm.configure, spn_target_output_path(s->mem, unit->metaprogram->scripts.configure));
    }
    if (unit->metaprogram->scripts.build) {
      spn_wasm_script_init(&unit->wasm.build, spn_target_output_path(s->mem, unit->metaprogram->scripts.build));
    }
  }
}

static spn_err_union_t add_metaprogram_targets(spn_session_t* s) {
  spn_build_unit_t* world = s->units.metaprogram;

  sp_da_for(world->packages, it) {
    spn_pkg_unit_t* unit = world->packages[it];
    spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, unit->id.pkg);
    if (!sp_da_empty(loaded->configure.source)) {
      spn_try_union(ensure_target(s, unit, &loaded->configure, SP_NULLPTR));
    }
    if (!sp_da_empty(loaded->build.source)) {
      spn_try_union(ensure_target(s, unit, &loaded->build, SP_NULLPTR));
    }
  }

  sp_da_for(world->packages, it) {
    spn_pkg_unit_t* unit = world->packages[it];
    if (spn_pkg_unit_is_script_host(unit)) {
      continue;
    }
    sp_str_om_for(unit->info->libs, jt) {
      spn_try_union(ensure_target(s, unit, sp_str_om_at(unit->info->libs, jt), SP_NULLPTR));
    }
  }

  sp_da(spn_target_unit_t*) targets = sp_da_new(s->mem, spn_target_unit_t*);
  collect_unit_targets(&targets, world->packages);
  spn_try_union(ensure_sibling_targets(s, &targets));
  spn_try_union(resolve_target_deps(s, targets));

  sp_da_for(targets, it) {
    if (targets[it]->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }
    create_target_objects(s, targets[it]);
  }
  sp_da_for(world->packages, it) {
    spn_target_unit_t* configure = world->packages[it]->scripts.configure;
    if (configure) {
      create_target_objects(s, configure);
      sp_assert(!sp_da_empty(configure->objects));
    }
  }

  sp_da_for(targets, it) {
    spn_try_union(build_target_plan(targets[it]));
  }
  sp_da_for(world->packages, it) {
    spn_target_unit_t* configure = world->packages[it]->scripts.configure;
    if (configure) {
      spn_try_union(build_target_plan(configure));
    }
  }

  init_wasm_scripts(s);
  return spn_result(SPN_OK);
}

static bool exe_name_reserved(sp_str_t name) {
  return sp_str_equal_cstr(name, "store") || sp_str_equal_cstr(name, "work") || sp_str_equal_cstr(name, "test");
}

static bool is_root_target(spn_session_t* s, spn_build_plan_t* plan, spn_target_unit_t* target) {
  sp_da_for(plan->roots, it) {
    if (spn_session_get_target_unit(s, plan->roots[it]) == target) {
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
    (target_rule_requests_name(&selection->lib, name) && sp_str_om_has(pkg->libs, name)) ||
    (target_rule_requests_name(&selection->bin, name) && sp_str_om_has(pkg->exes, name)) ||
    (target_rule_requests_name(&selection->test, name) && sp_str_om_has(pkg->tests, name)) ||
    (target_rule_requests_name(&selection->script, name) && sp_str_om_has(pkg->scripts, name));
}

static spn_err_union_t validate_target_selection(const spn_target_selection_t* selection, const spn_pkg_info_t* pkg) {
  const spn_target_rule_t* rules [] = {
    &selection->lib,
    &selection->bin,
    &selection->test,
    &selection->script,
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
      return (spn_err_union_t) {
        .kind = SPN_ERR_TARGET_SELECTION,
        .target = { .name = name },
      };
    }
  }
  return spn_result(SPN_OK);
}

static spn_err_union_t add_plan_targets(spn_session_t* s, spn_build_plan_t* plan, spn_pkg_unit_t* pkg, spn_target_map_t targets) {
  sp_str_om_for(targets, it) {
    spn_target_info_t* info = sp_str_om_at(targets, it);
    if (!spn_target_selection_pass(&plan->selection, info)) {
      continue;
    }

    bool staged_at_root = info->kind == SPN_TARGET_EXE || info->kind == SPN_TARGET_SCRIPT;
    if (staged_at_root && exe_name_reserved(info->name)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_TARGET_RESERVED,
        .target = {
          .pkg = pkg->info->name,
          .name = info->name,
        },
      };
    }

    spn_target_unit_t* target = SP_NULLPTR;
    spn_try_union(ensure_target(s, pkg, info, &target));
    if (!is_root_target(s, plan, target)) {
      sp_da_push(plan->roots, target->id);
    }
  }
  return spn_result(SPN_OK);
}

static spn_err_union_t add_plan_root_targets(spn_session_t* s) {
  spn_pkg_id_t root = spn_session_root_pkg(s);

  sp_da_for(s->plans, it) {
    spn_build_plan_t* plan = &s->plans[it];
    sp_da_for(plan->build->packages, jt) {
      spn_pkg_unit_t* pkg = plan->build->packages[jt];
      if (spn_pkg_id_eq(pkg->id.pkg, root)) {
        continue;
      }
      sp_str_om_for(pkg->info->libs, kt) {
        spn_try_union(ensure_target(s, pkg, sp_str_om_at(pkg->info->libs, kt), SP_NULLPTR));
      }
    }

    spn_pkg_unit_t* pkg = spn_session_find_pkg_unit(s, plan->build, root);
    sp_assert(pkg);
    spn_try_union(validate_target_selection(&plan->selection, pkg->info));
    spn_try_union(add_plan_targets(s, plan, pkg, pkg->info->libs));
    spn_try_union(add_plan_targets(s, plan, pkg, pkg->info->exes));
    spn_try_union(add_plan_targets(s, plan, pkg, pkg->info->scripts));
    spn_try_union(add_plan_targets(s, plan, pkg, pkg->info->tests));
  }
  return spn_result(SPN_OK);
}

static spn_err_union_t add_target_build_targets(spn_session_t* s) {
  spn_try_union(add_plan_root_targets(s));

  sp_da(spn_target_unit_t*) targets = sp_da_new(s->mem, spn_target_unit_t*);
  sp_da_for(s->plans, it) {
    collect_unit_targets(&targets, s->plans[it].build->packages);
  }
  spn_try_union(ensure_sibling_targets(s, &targets));
  spn_try_union(resolve_target_deps(s, targets));

  sp_da_for(targets, it) {
    if (targets[it]->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }
    create_target_objects(s, targets[it]);
  }

  sp_om_for(s->units.objects, it) {
    spn_compile_unit_t* object = sp_om_at(s->units.objects, it);
    spn_toolchain_info_t* toolchain = object->target->pkg->build->toolchain->info;
    if (object->lang != SPN_LANG_CXX || spn_toolchain_has_cxx(toolchain)) {
      continue;
    }
    return (spn_err_union_t) { .kind = SPN_ERR_TOOLCHAIN_NO_CXX, .toolchain = { .name = toolchain->name } };
  }

  sp_da_for(targets, it) {
    spn_try_union(build_target_plan(targets[it]));
  }
  return spn_result(SPN_OK);
}

spn_err_union_t spn_units_add_targets(spn_session_t* s, spn_unit_scope_t scope) {
  switch (scope) {
    case SPN_UNIT_SCOPE_METAPROGRAM: return add_metaprogram_targets(s);
    case SPN_UNIT_SCOPE_TARGET:      return add_target_build_targets(s);
  }
  sp_unreachable_return(spn_result(SPN_ERROR));
}
