#include "unit/unit.h"

#include "sp.h"
#include "macro/macro.h"
#include "str/str.h"

#include "ctx/types.h"

#include "compiler/driver.h"
#include "dag/dag.h"
#include "error/error.h"
#include "enum/enum.h"
#include "external/wasm/wasm.h"
#include "filter/filter.h"
#include "intern/intern.h"
#include "paths/paths.h"
#include "pkg/id.h"
#include "target/mutate.h"
#include "pkg/pkg.h"
#include "session/invocation.h"
#include "session/session.h"
#include "target/select.h"
#include "graph/build.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

static spn_target_unit_t* add_target(spn_session_t* s, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  spn_target_unit_id_t id = {
    .pkg = pkg->id,
    .target = sp_intern_get_or_insert(s->ctx->intern, info->name),
    .kind = info->kind,
  };

  sp_om_insert(s->units.targets, id, SP_ZERO_STRUCT(spn_target_unit_t));
  spn_target_unit_t* target = sp_om_back(s->units.targets);
  target->id = id;
  target->pkg = pkg;
  target->info = info;
  sp_da_init(s->mem, target->objects);
  sp_da_init(s->mem, target->deps);

  switch (info->kind) {
    case SPN_TARGET_KIND_LIB: {
      sp_da_push(pkg->targets, target);
      sp_da_push(pkg->libs, target);
      break;
    }
    case SPN_TARGET_KIND_EXE:
    case SPN_TARGET_KIND_SCRIPT:
    case SPN_TARGET_KIND_TEST:
    case SPN_TARGET_KIND_EXAMPLE: {
      sp_da_push(pkg->targets, target);
      break;
    }
    case SPN_TARGET_KIND_CONFIGURE_METAPROGRAM: {
      sp_assert(pkg->build == s->units.metaprogram);
      pkg->scripts.configure = target;
      break;
    }
    case SPN_TARGET_KIND_BUILD_METAPROGRAM: {
      sp_assert(pkg->build == s->units.metaprogram);
      pkg->scripts.build = target;
      sp_da_push(pkg->targets, target);
      break;
    }
  }

  return target;
}

static spn_err_t set_target_kind(spn_session_t* s, spn_target_unit_t* target) {
  spn_target_info_t* info = target->info;

  switch (info->kind) {
    case SPN_TARGET_KIND_EXE:
    case SPN_TARGET_KIND_SCRIPT:
    case SPN_TARGET_KIND_TEST:
    case SPN_TARGET_KIND_EXAMPLE: {
      target->kind = SPN_CC_OUTPUT_EXE;
      return SPN_OK;
    }
    case SPN_TARGET_KIND_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_KIND_BUILD_METAPROGRAM: {
      target->kind = SPN_CC_OUTPUT_REACTOR;
      return SPN_OK;
    }
    case SPN_TARGET_KIND_LIB: {
      if (spn_linkage_set_has(info->linkages, SPN_LIB_KIND_OBJECT) || info->no_link) {
        target->lib_kind = spn_linkage_set_default(info->linkages);
      }
      else {
        spn_kind_query_t query = {
          .config = spn_session_config_kind(s, target->pkg->info->name),
          .linkage = target->pkg->build->profile.linkage,
        };

        if (spn_target_select_lib_kind(info, query, &target->lib_kind)) {
          return spn_err_emit(s->ctx, (spn_err_union_t) {
            .kind = SPN_ERR_TARGET_LINKAGE,
            .target = {
              .pkg = target->pkg->info->name,
              .requested = spn_linkage_to_str(query.config.some ? query.config.value : query.linkage),
              .requester = query.config.some ? sp_str_lit("the root manifest") : sp_str_lit("the profile"),
            },
          });
        }
      }

      switch (target->lib_kind) {
        case SPN_LIB_KIND_STATIC: target->kind = SPN_CC_OUTPUT_STATIC_LIB; break;
        case SPN_LIB_KIND_SHARED: target->kind = SPN_CC_OUTPUT_SHARED_LIB; break;
        case SPN_LIB_KIND_SOURCE:
        case SPN_LIB_KIND_OBJECT: target->kind = SPN_CC_OUTPUT_OBJECT; break;
        case SPN_LIB_KIND_NONE: break;
      }
      return SPN_OK;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

static spn_err_t ensure_target(spn_session_t* s, spn_pkg_unit_t* pkg, spn_target_info_t* info, spn_target_unit_t** result) {
  spn_target_unit_t* target = spn_session_find_target_in_pkg(s, pkg, info->name, info->kind);
  if (target && target->info != info) {
    return spn_err_emit(s->ctx, (spn_err_union_t) {
      .kind = SPN_ERR_TARGET_DUPLICATE,
      .target = {
        .pkg = pkg->info->name,
        .name = info->name,
      },
    });
  }
  if (!target) {
    target = add_target(s, pkg, info);
    spn_try(set_target_kind(s, target));
  }
  if (result) *result = target;
  return SPN_OK;
}

static bool has_source_file(sp_da(spn_path_t) source, spn_path_t path) {
  sp_da_for(source, it) {
    if (spn_path_equal(source[it], path)) {
      return true;
    }
  }
  return false;
}

static void collect_source_glob(sp_mem_t mem, spn_path_t pattern, sp_da(spn_path_t)* source) {
  sp_da(spn_path_t) matches = sp_da_new(mem, spn_path_t);
  spn_dag_glob(mem, &spn.roots, pattern, SP_NULLPTR, &matches);

  sp_da_for(matches, it) {
    spn_path_t match = spn_path_canonicalize(mem, &spn.roots, matches[it]);
    if (has_source_file(*source, match)) {
      continue;
    }
    sp_da_push(*source, match);
  }
}

static sp_da(spn_path_t) collect_target_source(sp_mem_t mem, spn_pkg_unit_t* pkg, spn_target_unit_t* target) {
  sp_da(spn_path_t) source = sp_da_new(mem, spn_path_t);

  sp_da_for(target->info->source, it) {
    spn_path_t path = target->info->source[it];
    spn_tree_rel_t rel = spn_tree_rel(pkg->paths.roots, path);
    if (rel.tree != SPN_TREE_NONE && sp_fs_is_glob(rel.sub)) {
      collect_source_glob(mem, path, &source);
      continue;
    }
    if (has_source_file(source, path)) {
      continue;
    }
    sp_da_push(source, path);
  }

  return source;
}

typedef struct {
  sp_str_t prefix;
  sp_str_t path;
} object_name_t;

static object_name_t object_name(spn_tree_roots_t roots, spn_path_t path) {
  spn_tree_rel_t rel = spn_tree_rel(roots, path);
  switch (rel.tree) {
    case SPN_TREE_MANIFEST: return (object_name_t) { .prefix = sp_str_lit("manifest"), .path = rel.sub };
    case SPN_TREE_SOURCE:   return (object_name_t) { .prefix = sp_str_lit("source"), .path = rel.sub };
    case SPN_TREE_NONE:     return (object_name_t) { .prefix = spn_path_root_label(path.root), .path = rel.sub };
  }

  sp_unreachable_return(sp_zero_struct(object_name_t));
}

static void create_target_objects(spn_session_t* s, spn_target_unit_t* target) {
  spn_pkg_unit_t* pkg = target->pkg;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(spn_path_t) source = collect_target_source(scratch.mem, pkg, target);
  spn_path_t target_dir = spn_target_unit_object_dir(s->mem, target);

  sp_da_for(source, it) {
    spn_path_t file = spn_path_copy(s->mem, source[it]);
    object_name_t name = object_name(pkg->paths.roots, file);

    spn_lang_t lang = spn_lang_from_path(name.path);

    spn_path_t object_dir = spn_path_join(s->mem, target_dir, name.prefix);
    spn_path_t object_path = spn_path_join(s->mem, object_dir, sp_fmt(scratch.mem, "{}.o", SP_FMT_STR(name.path)).value);
    spn_compile_unit_id_t id = {
      .target = target->id,
      .source = sp_intern_get_or_insert(s->ctx->intern, spn_path_str(&spn.roots, scratch.mem, file)),
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

static spn_path_t static_archive_path(sp_mem_t mem, spn_target_unit_t* lib) {
  spn_profile_info_t* profile = &lib->pkg->build->profile;
  spn_triple_t triple = { profile->arch, profile->os, profile->abi };
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  sp_str_t file_name = spn_triple_lib_file_name(s.mem, triple, lib->info->name, SP_OS_LIB_STATIC);
  spn_path_t path = spn_path_join(mem, lib->pkg->paths.lib, file_name);
  sp_mem_end_scratch(s);
  return path;
}

typedef sp_str_ht(u8) link_str_set_t;

static void push_unique(link_str_set_t* seen, sp_da(sp_str_t)* result, sp_da(sp_str_t) values) {
  sp_da_for(values, it) {
    if (sp_str_ht_exists(*seen, values[it])) {
      continue;
    }
    sp_str_ht_insert(*seen, values[it], (u8)true);
    sp_da_push(*result, values[it]);
  }
}

static spn_err_t render_compile_bases(sp_mem_t mem, spn_target_unit_t* target) {
  sp_da_for(target->objects, it) {
    spn_compile_unit_t* unit = target->objects[it];
    spn_try(spn_build_render_compile(mem, unit, &unit->invocation));
  }
  return SPN_OK;
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
  link_str_set_t seen;
  sp_str_ht_init(s.mem, seen);

  push_unique(&seen, frameworks, target->info->macos.frameworks);

  sp_da_for(closure, it) {
    spn_closure_entry_t* entry = &closure[it];
    if (entry->links_code) {
      push_unique(&seen, frameworks, entry->pkg->info->macos.frameworks);
    }
    sp_da_for(entry->targets, lt) {
      spn_target_unit_t* lib = entry->targets[lt];
      if (lib->info->no_link) continue;
      if (lib->lib_kind == SPN_LIB_KIND_SHARED) continue;
      push_unique(&seen, frameworks, lib->info->macos.frameworks);
    }
  }
  sp_mem_end_scratch(s);
}

static void link_plan_system_libs(spn_target_unit_t* target, sp_da(spn_closure_entry_t) closure, sp_da(sp_str_t)* system_libs) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  link_str_set_t seen;
  sp_str_ht_init(s.mem, seen);

  push_unique(&seen, system_libs, target->info->system_deps);

  sp_da_for(closure, it) {
    spn_closure_entry_t* entry = &closure[it];
    push_unique(&seen, system_libs, entry->pkg->info->system_deps);
    sp_da_for(entry->targets, lt) {
      spn_target_unit_t* lib = entry->targets[lt];
      if (lib->info->no_link) continue;
      if (lib->lib_kind == SPN_LIB_KIND_SHARED) continue;
      push_unique(&seen, system_libs, lib->info->system_deps);
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
  sp_da_init(mem, plan.archives);
  sp_da_init(mem, plan.cc.private_libs);
  sp_da_init(mem, plan.cc.frameworks);

  link_plan_frameworks(target, closure, &plan.cc.frameworks);
  link_plan_system_libs(target, closure, &plan.cc.system_libs);

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
        sp_da_push(plan.archives, static_archive_path(mem, lib->lib));
        break;
      }
    }
  }

  sp_mem_end_scratch(s);
  return plan;
}

spn_err_t spn_target_link_invocation(sp_mem_t mem, spn_target_unit_t* target, const spn_cc_link_files_t* files, spn_invocation_t* invocation) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &target->pkg->build->toolchain->cc;

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      spn_cc_archive_files_t archive_files = {
        .output = files->output,
        .objects = files->objects,
      };
      spn_try(spn_cc_render_archive(mem, toolchain, profile, &archive_files, invocation));
      break;
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      spn_cc_link_files_t linked = *files;
      linked.whole_archives = target->link.archives;
      spn_try(spn_cc_render_link(mem, toolchain, profile, &target->link.cc, &linked, invocation));
      break;
    }
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  invocation->cwd = target->pkg->paths.work;
  return SPN_OK;
}

static spn_err_t build_target_plan(spn_target_unit_t* target) {
  spn_pkg_unit_t* pkg = target->pkg;
  spn_profile_info_t* profile = &pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &pkg->build->toolchain->cc;

  target->link = link_plan(target);
  spn_try(render_compile_bases(pkg->session->mem, target));

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      return spn_cc_validate_archive(toolchain, profile);
    }
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      spn_try(spn_cc_validate_archive(toolchain, profile));
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.cc.frameworks));
    }
    case SPN_CC_OUTPUT_EXE: {
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.cc.frameworks));
    }
    case SPN_CC_OUTPUT_OBJECT: {
      return SPN_OK;
    }
  }

  sp_unreachable_return(SPN_ERROR);
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

static spn_err_t ensure_sibling_targets(spn_session_t* s, sp_da(spn_target_unit_t*)* targets) {
  sp_for(it, sp_da_size(*targets)) {
    spn_target_unit_t* unit = (*targets)[it];
    sp_da_for(unit->info->deps, jt) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[jt]);
      if (find_dep_unit(s, unit->pkg, qualified)) {
        continue;
      }
      if (spn_session_find_target_in_pkg(s, unit->pkg, unit->info->deps[jt], SPN_TARGET_KIND_LIB)) {
        continue;
      }
      sp_str_t name = spn_intern(unit->info->deps[jt]);
      if (!sp_str_om_has(unit->pkg->info->libs, name)) {
        continue;
      }
      spn_target_info_t* info = sp_str_om_get(unit->pkg->info->libs, name);
      spn_target_unit_t* target = SP_NULLPTR;
      spn_try(ensure_target(s, unit->pkg, info, &target));
      sp_da_push(*targets, target);
    }
  }
  return SPN_OK;
}

static spn_err_t resolve_target_deps(spn_session_t* s, sp_da(spn_target_unit_t*) targets) {
  sp_da_for(targets, it) {
    spn_target_unit_t* unit = targets[it];
    sp_da_for(unit->info->deps, jt) {
      sp_str_t qualified = spn_pkg_canonicalize_name(unit->info->deps[jt]);
      if (find_dep_unit(s, unit->pkg, qualified)) {
        continue;
      }

      spn_target_unit_t* target = spn_session_find_target_in_pkg(s, unit->pkg, unit->info->deps[jt], SPN_TARGET_KIND_LIB);
      if (!target) {
        return spn_err_emit(s->ctx, (spn_err_union_t) {
          .kind = SPN_ERR_TARGET_DEP,
          .target = { .name = unit->info->deps[jt] },
        });
      }
      sp_da_push(unit->deps, target);
    }
  }
  return SPN_OK;
}

static void init_wasm_scripts(spn_session_t* s) {
  sp_om_for(s->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(s->units.packages, it);
    if (!unit->metaprogram) {
      continue;
    }
    const spn_path_roots_t* roots = &spn.roots;
    if (unit->metaprogram->scripts.configure) {
      spn_wasm_script_init(&unit->wasm.configure, spn_path_str(roots, s->mem, spn_target_output_path(s->mem, unit->metaprogram->scripts.configure)));
    }
    if (unit->metaprogram->scripts.build) {
      spn_wasm_script_init(&unit->wasm.build, spn_path_str(roots, s->mem, spn_target_output_path(s->mem, unit->metaprogram->scripts.build)));
    }
  }
}

static spn_err_t add_metaprogram_targets(spn_session_t* s) {
  spn_build_unit_t* world = s->units.metaprogram;

  sp_da_for(world->packages, it) {
    spn_pkg_unit_t* unit = world->packages[it];
    spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, unit->id.pkg);
    if (!sp_da_empty(loaded->configure.source)) {
      spn_try(ensure_target(s, unit, &loaded->configure, SP_NULLPTR));
    }
    if (!sp_da_empty(loaded->build.source)) {
      spn_try(ensure_target(s, unit, &loaded->build, SP_NULLPTR));
    }
  }

  sp_da_for(world->packages, it) {
    spn_pkg_unit_t* unit = world->packages[it];
    if (spn_pkg_unit_is_script_host(unit)) {
      continue;
    }
    sp_str_om_for(unit->info->libs, jt) {
      spn_try(ensure_target(s, unit, sp_str_om_at(unit->info->libs, jt), SP_NULLPTR));
    }
  }

  sp_da(spn_target_unit_t*) targets = sp_da_new(s->mem, spn_target_unit_t*);
  collect_unit_targets(&targets, world->packages);
  spn_try(ensure_sibling_targets(s, &targets));
  spn_try(resolve_target_deps(s, targets));

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
    spn_try(build_target_plan(targets[it]));
  }
  sp_da_for(world->packages, it) {
    spn_target_unit_t* configure = world->packages[it]->scripts.configure;
    if (configure) {
      spn_try(build_target_plan(configure));
    }
  }

  init_wasm_scripts(s);
  return SPN_OK;
}

static bool exe_name_reserved(sp_str_t name) {
  return sp_str_equal_cstr(name, "store") || sp_str_equal_cstr(name, ".spn") || sp_str_equal_cstr(name, "test") || sp_str_equal_cstr(name, "example");
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
  sp_for(it, rule->names.count) {
    if (sp_str_equal(rule->names.items[it], name)) {
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
    (target_rule_requests_name(&selection->script, name) && sp_str_om_has(pkg->scripts, name)) ||
    (target_rule_requests_name(&selection->example, name) && sp_str_om_has(pkg->examples, name));
}

static spn_err_t validate_target_selection(spn_session_t* s, const spn_target_selection_t* selection, const spn_pkg_info_t* pkg) {
  const spn_target_rule_t* rules [] = {
    &selection->lib,
    &selection->bin,
    &selection->test,
    &selection->script,
    &selection->example,
  };
  sp_carr_for(rules, rt) {
    const spn_target_rule_t* rule = rules[rt];
    if (rule->kind != SPN_TARGET_RULE_NAMED) {
      continue;
    }
    sp_for(it, rule->names.count) {
      sp_str_t name = rule->names.items[it];
      if (target_selection_matches_name(selection, pkg, name)) {
        continue;
      }
      return spn_err_emit(s->ctx, (spn_err_union_t) {
        .kind = SPN_ERR_TARGET_SELECTION,
        .target = { .name = name },
      });
    }
  }
  return SPN_OK;
}

static spn_err_t add_plan_targets(spn_session_t* s, spn_build_plan_t* plan, spn_pkg_unit_t* pkg, spn_target_map_t targets) {
  sp_str_om_for(targets, it) {
    spn_target_info_t* info = sp_str_om_at(targets, it);
    if (!spn_target_selection_pass(&plan->selection, info)) {
      continue;
    }

    bool staged_at_root = info->kind == SPN_TARGET_KIND_EXE || info->kind == SPN_TARGET_KIND_SCRIPT;
    if (staged_at_root && exe_name_reserved(info->name)) {
      return spn_err_emit(s->ctx, (spn_err_union_t) {
        .kind = SPN_ERR_TARGET_RESERVED,
        .target = {
          .pkg = pkg->info->name,
          .name = info->name,
        },
      });
    }

    spn_target_unit_t* target = SP_NULLPTR;
    spn_try(ensure_target(s, pkg, info, &target));
    if (!is_root_target(s, plan, target)) {
      sp_da_push(plan->roots, target->id);
    }
  }
  return SPN_OK;
}

static spn_err_t add_plan_root_targets(spn_session_t* s) {
  spn_pkg_id_t root = spn_session_root_pkg(s);

  sp_da_for(s->plans, it) {
    spn_build_plan_t* plan = &s->plans[it];
    sp_da_for(plan->build->packages, jt) {
      spn_pkg_unit_t* pkg = plan->build->packages[jt];
      if (spn_pkg_id_eq(pkg->id.pkg, root)) {
        continue;
      }
      sp_str_om_for(pkg->info->libs, kt) {
        spn_try(ensure_target(s, pkg, sp_str_om_at(pkg->info->libs, kt), SP_NULLPTR));
      }
    }

    spn_pkg_unit_t* pkg = spn_session_find_pkg_unit(s, plan->build, root);
    sp_assert(pkg);
    spn_try(validate_target_selection(s, &plan->selection, pkg->info));
    spn_try(add_plan_targets(s, plan, pkg, pkg->info->libs));
    spn_try(add_plan_targets(s, plan, pkg, pkg->info->exes));
    spn_try(add_plan_targets(s, plan, pkg, pkg->info->scripts));
    spn_try(add_plan_targets(s, plan, pkg, pkg->info->tests));
    spn_try(add_plan_targets(s, plan, pkg, pkg->info->examples));
  }
  return SPN_OK;
}

static spn_err_t add_target_build_targets(spn_session_t* s) {
  spn_try(add_plan_root_targets(s));

  sp_da(spn_target_unit_t*) targets = sp_da_new(s->mem, spn_target_unit_t*);
  sp_da_for(s->plans, it) {
    collect_unit_targets(&targets, s->plans[it].build->packages);
  }
  spn_try(ensure_sibling_targets(s, &targets));
  spn_try(resolve_target_deps(s, targets));

  sp_da_for(targets, it) {
    if (targets[it]->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }
    create_target_objects(s, targets[it]);
  }

  sp_om_for(s->units.objects, it) {
    spn_compile_unit_t* object = sp_om_at(s->units.objects, it);
    spn_toolchain_unit_t* toolchain = object->target->pkg->build->toolchain;
    if (object->lang != SPN_LANG_CXX || !spn_arg_empty(toolchain->cc.cxx.program)) {
      continue;
    }
    return spn_err_emit(s->ctx, (spn_err_union_t) { .kind = SPN_ERR_TOOLCHAIN_NO_CXX, .toolchain = { .name = toolchain->info->name } });
  }

  sp_da_for(targets, it) {
    spn_try(build_target_plan(targets[it]));
  }
  return SPN_OK;
}

spn_err_t spn_units_add_targets(spn_session_t* s, spn_unit_scope_t scope) {
  switch (scope) {
    case SPN_UNIT_SCOPE_METAPROGRAM: return add_metaprogram_targets(s);
    case SPN_UNIT_SCOPE_TARGET:      return add_target_build_targets(s);
  }
  sp_unreachable_return(SPN_ERROR);
}
