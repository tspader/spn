#include "fuzz.h"
#include "semver/convert.h"
#include "yyjson.h"

#define SP_TEMPLATE_IMPLEMENTATION
#include "sp_template/sp_template.h"

static sp_str_t fz_repo_root(sp_mem_t mem) {
  sp_str_t path = sp_fs_get_exe_path(mem);
  while (true) {
    sp_assert(!sp_str_empty(path));
    if (sp_str_equal(sp_fs_get_stem(path), sp_str_lit("spn"))) {
      return path;
    }
    path = sp_fs_parent_path(path);
  }
}

static bool fz_load_template(sp_mem_t mem, sp_str_t* tmpl) {
  sp_str_t root = fz_repo_root(mem);
  sp_str_t path = sp_fs_join_path(mem, root, sp_str_lit("test/fuzz/resolver/templates/repro.tmpl"));
  return !sp_io_read_file(mem, path, tmpl);
}

static sp_str_t fz_root_attrs(fz_dep_t dep) {
  return dep.private ? sp_str_lit(", private = true") : sp_str_lit("");
}

static const c8* fz_linkage_json(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_NONE:   return "none";
    case SPN_LIB_KIND_SHARED: return "shared";
    case SPN_LIB_KIND_STATIC: return "static";
    case SPN_LIB_KIND_SOURCE: return "source";
    case SPN_LIB_KIND_OBJECT: return "object";
  }
  sp_unreachable_return("none");
}

static const c8* fz_dep_kind_json(spn_index_dep_kind_t kind) {
  switch (kind) {
    case SPN_INDEX_DEP_NORMAL: return "normal";
    case SPN_INDEX_DEP_BUILD:  return "build";
    case SPN_INDEX_DEP_TEST:   return "test";
  }
  sp_unreachable_return("normal");
}

static sp_str_t fz_release_line(sp_mem_t mem, fz_universe_t* u, u32 pkg, fz_release_t* release) {
  yyjson_mut_doc* doc = yyjson_mut_doc_new(SP_NULLPTR);
  yyjson_mut_val* root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_obj_add_strcpy(doc, root, "namespace", "spn");
  yyjson_mut_obj_add_strcpy(doc, root, "name", sp_str_to_cstr(mem, fz_pkg_name(pkg)));
  yyjson_mut_obj_add_strcpy(doc, root, "version", sp_str_to_cstr(mem, spn_semver_to_str(mem, release->version)));
  yyjson_mut_obj_add_bool(doc, root, "yanked", false);

  if (!sp_da_empty(release->deps)) {
    yyjson_mut_val* deps = yyjson_mut_obj_add_arr(doc, root, "deps");
    sp_da_for(release->deps, it) {
      fz_dep_t dep = release->deps[it];
      yyjson_mut_val* entry = yyjson_mut_arr_add_obj(doc, deps);
      yyjson_mut_obj_add_strcpy(doc, entry, "namespace", "spn");
      yyjson_mut_obj_add_strcpy(doc, entry, "name", sp_str_to_cstr(mem, fz_pkg_name(dep.pkg)));
      yyjson_mut_obj_add_strcpy(doc, entry, "version", sp_str_to_cstr(mem, fz_range_render(mem, dep)));
      yyjson_mut_obj_add_strcpy(doc, entry, "kind", fz_dep_kind_json(dep.kind));
      if (dep.private) {
        yyjson_mut_obj_add_bool(doc, entry, "private", true);
      }
    }
  }

  spn_linkage_set_t linkages = u->pkgs[pkg].linkages;
  if (linkages.source || linkages.static_lib || linkages.shared) {
    yyjson_mut_val* targets = yyjson_mut_obj_add_arr(doc, root, "targets");
    yyjson_mut_val* target = yyjson_mut_arr_add_obj(doc, targets);
    yyjson_mut_obj_add_strcpy(doc, target, "name", sp_str_to_cstr(mem, fz_pkg_name(pkg)));
    yyjson_mut_val* names = yyjson_mut_obj_add_arr(doc, target, "linkages");
    if (linkages.source) {
      yyjson_mut_arr_add_strcpy(doc, names, "source");
    }
    if (linkages.static_lib) {
      yyjson_mut_arr_add_strcpy(doc, names, "static");
    }
    if (linkages.shared) {
      yyjson_mut_arr_add_strcpy(doc, names, "shared");
    }
  }

  c8* json = yyjson_mut_write(doc, 0, SP_NULLPTR);
  sp_str_t line = sp_str_copy(mem, sp_cstr_as_str(json));
  free(json);
  yyjson_mut_doc_free(doc);
  return line;
}

void fz_dump(fz_universe_t* u, u64 iter) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_mem_t mem = scratch.mem;

  sp_io_writer_t* out = sp_io_get_std_out();

  sp_str_t mode = u->planted ? sp_str_lit("planted") : sp_str_lit("free");
  if (u->profile.features) {
    mode = sp_str_lit("features");
  }
  if (u->profile.big) {
    mode = sp_fmt(mem, "big {}", sp_fmt_str(mode)).value;
  }
  if (u->profile.budget) {
    mode = sp_fmt(mem, "{}, budget {}", sp_fmt_str(mode), sp_fmt_uint(u->profile.budget)).value;
  }

  sp_str_t verdict = sp_str_lit("<unchecked>");
  if (!u->profile.features && !u->profile.big) {
    verdict = fz_oracle_sat(u) ? sp_str_lit("ok") : sp_str_lit("pkg_no_match");
  }

  sp_template_scope_t* scope = sp_template_scope_create(mem);
  sp_template_set(scope, sp_str_lit("mode"), mode);
  sp_template_set(scope, sp_str_lit("iter"), sp_fmt(mem, "{}", sp_fmt_uint(iter)).value);
  sp_template_set(scope, sp_str_lit("err"), verdict);
  sp_template_set(scope, sp_str_lit("linkage"), sp_cstr_as_str(fz_linkage_json(u->profile.linkage)));

  sp_template_list(scope, sp_str_lit("with_budget"));
  if (u->profile.budget) {
    sp_template_scope_t* budget = sp_template_push(scope, sp_str_lit("with_budget"));
    sp_template_set(budget, sp_str_lit("budget"), sp_fmt(mem, "{}", sp_fmt_uint(u->profile.budget)).value);
  }

  static const struct { spn_index_dep_kind_t kind; const c8* table; } root_tables [] = {
    { SPN_INDEX_DEP_NORMAL, "package" },
    { SPN_INDEX_DEP_BUILD,  "build" },
    { SPN_INDEX_DEP_TEST,   "test" },
  };

  sp_template_list(scope, sp_str_lit("root_kinds"));
  sp_carr_for(root_tables, kt) {
    bool any = false;
    sp_da_for(u->roots, it) {
      any = any || u->roots[it].kind == root_tables[kt].kind;
    }
    if (!any) {
      continue;
    }

    sp_template_scope_t* table = sp_template_push(scope, sp_str_lit("root_kinds"));
    sp_template_set(table, sp_str_lit("table"), sp_str_view(root_tables[kt].table));
    sp_template_list(table, sp_str_lit("deps"));
    sp_da_for(u->roots, it) {
      if (u->roots[it].kind != root_tables[kt].kind) {
        continue;
      }
      sp_template_scope_t* dep = sp_template_push(table, sp_str_lit("deps"));
      sp_template_set(dep, sp_str_lit("name"), fz_pkg_name(u->roots[it].pkg));
      sp_template_set(dep, sp_str_lit("range"), fz_range_render(mem, u->roots[it]));
      sp_template_set(dep, sp_str_lit("attrs"), fz_root_attrs(u->roots[it]));
    }
  }

  sp_template_list(scope, sp_str_lit("config"));
  sp_da_for(u->pkgs, it) {
    if (!u->pkgs[it].has_config) {
      continue;
    }
    sp_template_scope_t* entry = sp_template_push(scope, sp_str_lit("config"));
    sp_template_set(entry, sp_str_lit("name"), fz_pkg_name((u32)it));
    sp_template_set(entry, sp_str_lit("kind"), sp_str_view(fz_linkage_json(u->pkgs[it].config)));
  }

  sp_template_list(scope, sp_str_lit("pkgs"));
  sp_da_for(u->pkgs, it) {
    sp_template_scope_t* pkg = sp_template_push(scope, sp_str_lit("pkgs"));
    sp_template_set(pkg, sp_str_lit("name"), fz_pkg_name((u32)it));

    sp_template_list(pkg, sp_str_lit("releases"));
    sp_da_for(u->pkgs[it].releases, rt) {
      sp_template_scope_t* rel = sp_template_push(pkg, sp_str_lit("releases"));
      sp_template_set(rel, sp_str_lit("line"), fz_release_line(mem, u, (u32)it, &u->pkgs[it].releases[rt]));
    }
  }

  sp_str_t tmpl = sp_zero;
  if (fz_load_template(mem, &tmpl)) {
    sp_template_render(out, tmpl, scope, SP_NULLPTR);
  }
  else {
    sp_fmt_io(out, "fuzz: repro template missing; replay with --iter {}\n", sp_fmt_uint(iter));
  }

  bool locals = false;
  sp_da_for(u->pkgs, it) {
    locals = locals || u->pkgs[it].local;
  }
  if (locals) {
    sp_fmt_io(out, "fuzz: universe holds local packages; the fixture renders them as index entries, replay with --iter {}\n", sp_fmt_uint(iter));
  }

  sp_mem_end_scratch(scratch);
}
