#include "unit.h"

void spn_wasm_script_init(spn_wasm_script_t* script, sp_str_t module) {
  *script = (spn_wasm_script_t) { .path = module };
}

sp_da(sp_str_t) test_str_list(sp_mem_t mem, const c8* const* items, u32 max) {
  sp_da(sp_str_t) list = sp_da_new(mem, sp_str_t);
  sp_for(it, max) {
    if (!items[it]) {
      break;
    }
    sp_da_push(list, sp_str_view(items[it]));
  }
  return list;
}

spn_pkg_id_t find_pkg_id(spn_session_t* s, unit_graph_test_t* g, const c8* name) {
  sp_carr_for(g->pkgs, it) {
    if (!g->pkgs[it].name) {
      break;
    }
    if (sp_str_equal_cstr(sp_str_view(g->pkgs[it].name), name)) {
      return (spn_pkg_id_t) {
        .qualified = sp_intern_get_or_insert(s->intern, sp_str_view(g->pkgs[it].name)),
        .hash = it + 1,
      };
    }
  }
  return sp_zero_struct(spn_pkg_id_t);
}

static spn_target_info_t lib_info(sp_mem_t mem, const unit_lib_t* spec) {
  spn_target_info_t info = sp_zero;
  info.name = sp_str_view(spec->name);
  info.kind = SPN_TARGET_LIB;
  info.linkages = spec->linkages;
  info.no_link = spec->no_link;
  info.source = test_str_list(mem, spec->source, UNIT_TEST_MAX_STRS);
  info.deps = test_str_list(mem, spec->deps, UNIT_TEST_MAX_STRS);
  info.macos.frameworks = test_str_list(mem, spec->frameworks, UNIT_TEST_MAX_STRS);
  info.macos.min_os = spec->min_os;
  return info;
}

static spn_build_unit_t* add_build(spn_session_t* s, spn_build_id_t id, const c8* root, spn_profile_info_t profile) {
  sp_om_insert(s->units.builds, id, sp_zero_struct(spn_build_unit_t));
  spn_build_unit_t* build = sp_om_back(s->units.builds);
  build->id = id;
  build->profile = profile;
  build->paths.root = sp_str_view(root);
  sp_da_init(s->mem, build->include);
  sp_da_init(s->mem, build->packages);

  spn_toolchain_info_t* info = sp_alloc_type(s->mem, spn_toolchain_info_t);
  info->name = sp_str_lit("test");
  info->driver = SPN_CC_DRIVER_GCC;
  info->compiler.program = sp_str_lit("cc");
  info->cxx.program = sp_str_lit("c++");
  info->linker.program = sp_str_lit("cc");
  info->archiver.program = sp_str_lit("ar");

  spn_toolchain_unit_t* toolchain = sp_alloc_type(s->mem, spn_toolchain_unit_t);
  toolchain->info = info;
  toolchain->cc = (spn_cc_toolchain_t) {
    .name = info->name,
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = info->compiler,
    .cxx = info->cxx,
    .linker = info->linker,
    .archiver = info->archiver,
    .archiver_driver = SPN_AR_DRIVER_GNU,
  };
  build->toolchain = toolchain;
  return build;
}

spn_session_t* build_session(sp_mem_t mem, unit_graph_test_t* g) {
  spn_session_t* s = sp_alloc_type(mem, spn_session_t);
  s->mem = mem;
  s->intern = sp_intern_new(mem);
  sp_ht_init(mem, s->resolve);
  sp_ht_init(mem, s->packages);
  sp_ht_init(mem, s->options);
  sp_ht_init(mem, s->fingerprints);
  sp_da_init(mem, s->plans);
  sp_om_new(s->units.builds);
  sp_om_new(s->units.packages);
  sp_om_new(s->units.targets);
  sp_om_new(s->units.objects);

  spn_profile_info_t profile = {
    .name = sp_str_lit("debug"),
    .toolchain = sp_str_lit("auto"),
    .standard = SPN_C11,
    .mode = SPN_BUILD_MODE_DEBUG,
    .os = g->os ? g->os : SPN_OS_LINUX,
    .arch = SPN_ARCH_X64,
    .abi = g->os == SPN_OS_MACOS ? SPN_ABI_NONE : SPN_ABI_GNU,
    .sysroot = g->sysroot ? sp_str_view(g->sysroot) : sp_str_lit(""),
  };

  s->units.target = add_build(s, 1, "/build/debug", profile);
  s->units.metaprogram = add_build(s, 2, "/build/wasm32-wasi", profile);

  u32 count = 0;
  sp_carr_detect_len(g->pkgs, count, g->pkgs[count].name);

  sp_for(it, count) {
    unit_pkg_t* pkg = &g->pkgs[it];
    spn_pkg_id_t id = find_pkg_id(s, g, pkg->name);

    spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
    info->name = sp_str_view(pkg->name);
    info->qualified = sp_str_view(pkg->name);
    info->system_deps = test_str_list(mem, pkg->system_deps, UNIT_TEST_MAX_STRS);
    info->macos.frameworks = test_str_list(mem, pkg->frameworks, UNIT_TEST_MAX_STRS);
    info->macos.min_os = pkg->min_os;

    sp_carr_for(pkg->libs, lt) {
      if (!pkg->libs[lt].name) {
        break;
      }
      spn_target_info_t lib = lib_info(mem, &pkg->libs[lt]);
      sp_str_om_insert(info->libs, lib.name, lib);
    }

    if (it == 0) {
      s->pkg = info;
    }

    spn_loaded_pkg_t loaded = {
      .source = it == 0 ? SPN_PKG_SOURCE_ROOT : SPN_PKG_SOURCE_FILE,
      .info = info,
    };
    if (pkg->scripts) {
      sp_da_init(mem, loaded.configure.source);
      sp_da_push(loaded.configure.source, sp_str_lit("configure.c"));
    }
    sp_ht_insert(s->packages, id, loaded);

    spn_resolved_pkg_t resolved = {
      .id = id,
      .source = loaded.source,
    };
    sp_da_init(mem, resolved.edges);
    sp_carr_for(pkg->deps, dt) {
      if (!pkg->deps[dt].to) {
        break;
      }
      spn_pkg_id_t to = find_pkg_id(s, g, pkg->deps[dt].to);
      SP_ASSERT(to.hash);
      sp_da_push(resolved.edges, ((spn_resolved_dep_t) {
        .id = to,
        .kind = pkg->deps[dt].kind,
        .private = pkg->deps[dt].private,
      }));
    }
    sp_ht_insert(s->resolve, id, resolved);
  }

  spn_build_plan_t plan = { .build = s->units.target };
  sp_da_init(mem, plan.roots);
  sp_da_push(s->plans, plan);

  return s;
}
