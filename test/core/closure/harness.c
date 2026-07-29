#include "closure.h"

sp_err_t expect_target_names(sp_test_t* t, sp_da(spn_target_unit_t*) targets, const c8* const* expect, u32 max) {
  u32 expected = 0;
  sp_for(it, max) {
    if (!expect[it]) {
      break;
    }
    expected++;
  }

  sp_must_eq(t, expected, sp_da_size(targets));
  sp_for(it, expected) {
    sp_expect_str_eq_c(t, targets[it]->info->name, expect[it]);
  }

  return SP_OK;
}

spn_pkg_unit_t* find_pkg(spn_pkg_unit_t** pkgs, u32 count, const c8* name) {
  sp_str_t needle = sp_str_view(name);
  sp_for(it, count) {
    if (sp_str_equal(pkgs[it]->info->name, needle)) {
      return pkgs[it];
    }
  }
  return SP_NULLPTR;
}

spn_target_unit_t* find_lib(spn_pkg_unit_t** pkgs, u32 count, const c8* name) {
  sp_str_t needle = sp_str_view(name);
  sp_for(it, count) {
    sp_da_for(pkgs[it]->libs, lt) {
      if (sp_str_equal(pkgs[it]->libs[lt]->info->name, needle)) {
        return pkgs[it]->libs[lt];
      }
    }
  }
  return SP_NULLPTR;
}

static spn_target_unit_t* add_lib(sp_mem_t mem, spn_pkg_unit_t* pkg, const c8* name, spn_linkage_t kind, bool no_link) {
  spn_target_info_t* info = sp_alloc_type(mem, spn_target_info_t);
  info->name = sp_str_view(name);
  info->no_link = no_link;

  spn_target_unit_t* lib = sp_alloc_type(mem, spn_target_unit_t);
  lib->info = info;
  lib->pkg = pkg;
  lib->lib_kind = kind;
  sp_da_init(mem, lib->deps);
  sp_da_push(pkg->libs, lib);
  return lib;
}

static spn_pkg_unit_t* build_pkg(sp_mem_t mem, spn_session_t* session, u32 id, closure_pkg_t spec) {
  spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
  info->name = sp_str_view(spec.name);

  spn_pkg_unit_t* pkg = sp_alloc_type(mem, spn_pkg_unit_t);
  pkg->id.pkg.qualified = id;
  pkg->info = info;
  pkg->session = session;
  sp_da_init(mem, pkg->libs);

  if (spec.kind != SPN_LIB_KIND_NONE) {
    add_lib(mem, pkg, spec.name, spec.kind, false);
  }
  sp_carr_for(spec.libs, it) {
    if (!spec.libs[it].name) {
      break;
    }
    add_lib(mem, pkg, spec.libs[it].name, spec.libs[it].kind, spec.libs[it].no_link);
  }

  return pkg;
}

closure_graph_t build_graph(closure_graph_test_t* t) {
  closure_graph_t g = sp_zero;
  g.mem = sp_mem_os_new();
  spn_session_t* session = sp_alloc_type(g.mem, spn_session_t);
  session->mem = g.mem;

  sp_carr_for(t->pkgs, it) {
    if (!t->pkgs[it].name) {
      break;
    }
    g.pkgs[it] = build_pkg(g.mem, session, it + 1, t->pkgs[it]);
    g.count++;
  }

  sp_for(it, g.count) {
    sp_da(spn_pkg_dep_t) deps = sp_da_new(g.mem, spn_pkg_dep_t);
    sp_carr_for(t->pkgs[it].deps, d) {
      if (!t->pkgs[it].deps[d].name) {
        break;
      }
      sp_da_push(deps, ((spn_pkg_dep_t) {
        .unit = find_pkg(g.pkgs, g.count, t->pkgs[it].deps[d].name),
        .kind = t->pkgs[it].deps[d].kind,
        .private = t->pkgs[it].deps[d].private,
      }));
    }
    g.pkgs[it]->deps = deps;

    sp_carr_for(t->pkgs[it].libs, lt) {
      if (!t->pkgs[it].libs[lt].name) {
        break;
      }
      spn_target_unit_t* lib = find_lib(g.pkgs, g.count, t->pkgs[it].libs[lt].name);
      sp_carr_for(t->pkgs[it].libs[lt].deps, d) {
        if (!t->pkgs[it].libs[lt].deps[d]) {
          break;
        }
        sp_da_push(lib->deps, find_lib(g.pkgs, g.count, t->pkgs[it].libs[lt].deps[d]));
      }
    }
  }

  spn_pkg_unit_t* root = find_pkg(g.pkgs, g.count, t->root);
  spn_target_info_t* exe_info = sp_alloc_type(g.mem, spn_target_info_t);
  exe_info->name = root->info->name;
  exe_info->kind = t->target;

  spn_target_unit_t* exe = sp_alloc_type(g.mem, spn_target_unit_t);
  exe->pkg = root;
  exe->info = exe_info;
  exe->lib_kind = SPN_LIB_KIND_NONE;
  sp_da_init(g.mem, exe->deps);
  sp_carr_for(t->root_deps, it) {
    if (!t->root_deps[it]) {
      break;
    }
    sp_da_push(exe->deps, find_lib(g.pkgs, g.count, t->root_deps[it]));
  }

  g.root = exe;
  return g;
}
