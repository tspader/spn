#include "unit/unit.h"

#include "ctx/types.h"
#include "paths/paths.h"

static sp_str_t target_kind_dir(spn_target_kind_t kind) {
  switch (kind) {
    case SPN_TARGET_KIND_LIB:                   return sp_str_lit("lib");
    case SPN_TARGET_KIND_EXE:                   return sp_str_lit("exe");
    case SPN_TARGET_KIND_SCRIPT:                return sp_str_lit("script");
    case SPN_TARGET_KIND_TEST:                  return sp_str_lit("test");
    case SPN_TARGET_KIND_EXAMPLE:               return sp_str_lit("example");
    case SPN_TARGET_KIND_CONFIGURE_METAPROGRAM: return sp_str_lit("configure");
    case SPN_TARGET_KIND_BUILD_METAPROGRAM:     return sp_str_lit("build");
  }
  sp_unreachable_return(sp_str_lit(""));
}

spn_path_t spn_target_unit_object_dir(sp_mem_t mem, spn_target_unit_t* target) {
  if (target->lib_kind == SPN_LIB_KIND_OBJECT) {
    return target->pkg->paths.lib;
  }
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  spn_path_t kind = spn_path_join(s.mem, target->pkg->paths.object, target_kind_dir(target->info->kind));
  spn_path_t dir = spn_path_join(mem, kind, target->info->name);
  sp_mem_end_scratch(s);
  return dir;
}

void spn_unit_paths_init(spn_pkg_unit_t* unit, spn_loaded_pkg_t* loaded) {
  spn_session_t* s = unit->session;
  sp_mem_t mem = s->mem;
  spn_build_unit_t* build = unit->build;

  unit->paths.roots = loaded->roots;

  switch (loaded->source) {
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE: {
      spn_path_t work = spn_path_join(mem, build->paths.root, sp_str_lit(".spn"));
      spn_path_t store = spn_path_join(mem, build->paths.root, sp_str_lit("store"));
      unit->paths.work = spn_path_join(mem, work, loaded->info->name);
      unit->paths.store = spn_path_join(mem, store, loaded->info->name);
      break;
    }
    case SPN_PKG_SOURCE_INDEX: {
      sp_str_t fingerprint = spn_unit_fingerprint_str(mem, spn_unit_fingerprint(s, build, unit->id.pkg));
      spn_path_t work = spn_path_join(mem, spn_path_from_root(SPN_PATH_ROOT_BUILD), loaded->info->qualified);
      spn_path_t store = spn_path_join(mem, spn_path_from_root(SPN_PATH_ROOT_STORE), loaded->info->qualified);
      unit->paths.work = spn_path_join(mem, work, fingerprint);
      unit->paths.store = spn_path_join(mem, store, fingerprint);
      break;
    }
  }

  unit->paths.work = spn_path_anchor(mem, &spn.roots, unit->paths.work);
  unit->paths.store = spn_path_anchor(mem, &spn.roots, unit->paths.store);

  unit->paths.include = spn_path_join(mem, unit->paths.store, SP_LIT("include"));
  unit->paths.bin = spn_path_join(mem, unit->paths.store, SP_LIT("bin"));
  unit->paths.lib = spn_path_join(mem, unit->paths.store, SP_LIT("lib"));
  unit->paths.vendor = spn_path_join(mem, unit->paths.store, SP_LIT("vendor"));

  unit->paths.object = spn_path_join(mem, unit->paths.work, SP_LIT("object"));

  unit->paths.stamp.dir = spn_path_join(mem, unit->paths.work, SP_LIT("stamp"));
  unit->paths.stamp.configure = spn_path_join(mem, unit->paths.stamp.dir, SP_LIT("configure.stamp"));
  unit->paths.stamp.package = spn_path_join(mem, unit->paths.stamp.dir, SP_LIT("package.stamp"));
}
