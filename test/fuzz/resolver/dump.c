#include "fuzz.h"

#define SP_TEMPLATE_IMPLEMENTATION
#include "sp/sp_template.h"

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

static sp_str_t fz_dep_attrs(sp_mem_t mem, fz_dep_t dep) {
  sp_str_t attrs = sp_str_lit("");
  switch (dep.kind) {
    case SPN_INDEX_DEP_NORMAL: break;
    case SPN_INDEX_DEP_BUILD:  attrs = sp_fmt(mem, "{}, .kind = SPN_INDEX_DEP_BUILD", sp_fmt_str(attrs)).value; break;
    case SPN_INDEX_DEP_TEST:   attrs = sp_fmt(mem, "{}, .kind = SPN_INDEX_DEP_TEST", sp_fmt_str(attrs)).value; break;
  }
  if (dep.private) {
    attrs = sp_fmt(mem, "{}, .private = true", sp_fmt_str(attrs)).value;
  }
  return attrs;
}

static sp_str_t fz_root_attrs(sp_mem_t mem, fz_dep_t dep) {
  sp_str_t attrs = sp_str_lit("");
  switch (dep.kind) {
    case SPN_INDEX_DEP_NORMAL: break;
    case SPN_INDEX_DEP_BUILD:  attrs = sp_fmt(mem, "{}, .kind = SPN_DEP_KIND_BUILD", sp_fmt_str(attrs)).value; break;
    case SPN_INDEX_DEP_TEST:   attrs = sp_fmt(mem, "{}, .kind = SPN_DEP_KIND_TEST", sp_fmt_str(attrs)).value; break;
  }
  if (dep.private) {
    attrs = sp_fmt(mem, "{}, .private = true", sp_fmt_str(attrs)).value;
  }
  return attrs;
}

static sp_str_t fz_linkage_name(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_NONE:   return sp_str_lit("SPN_LIB_KIND_NONE");
    case SPN_LIB_KIND_SHARED: return sp_str_lit("SPN_LIB_KIND_SHARED");
    case SPN_LIB_KIND_STATIC: return sp_str_lit("SPN_LIB_KIND_STATIC");
    case SPN_LIB_KIND_SOURCE: return sp_str_lit("SPN_LIB_KIND_SOURCE");
    case SPN_LIB_KIND_OBJECT: return sp_str_lit("SPN_LIB_KIND_OBJECT");
  }
  sp_unreachable_return(sp_str_lit("SPN_LIB_KIND_NONE"));
}

static sp_str_t fz_render_linkages(sp_mem_t mem, spn_linkage_set_t linkages) {
  sp_str_t rendered = sp_str_lit("");
  if (linkages.source) {
    rendered = sp_fmt(mem, "{}SPN_LIB_KIND_SOURCE, ", sp_fmt_str(rendered)).value;
  }
  if (linkages.static_lib) {
    rendered = sp_fmt(mem, "{}SPN_LIB_KIND_STATIC, ", sp_fmt_str(rendered)).value;
  }
  if (linkages.shared) {
    rendered = sp_fmt(mem, "{}SPN_LIB_KIND_SHARED, ", sp_fmt_str(rendered)).value;
  }
  return rendered;
}

void fz_dump(fz_universe_t* u, u64 iter) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_mem_t mem = scratch.mem;

  sp_io_stream_writer_t out = sp_io_get_std_out();

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

  if (sp_da_size(u->pkgs) > 8) {
    sp_fmt_io(&out.base, "fuzz repro: mode={} iter={}: {} pkgs exceeds the fixture's 8, replay with --iter {}\n",
      sp_fmt_str(mode), sp_fmt_uint(iter), sp_fmt_uint(sp_da_size(u->pkgs)), sp_fmt_uint(iter));
    sp_mem_end_scratch(scratch);
    return;
  }

  sp_str_t verdict = sp_str_lit("<unchecked>");
  if (!u->profile.features && !u->profile.big) {
    verdict = fz_oracle_sat(u) ? sp_str_lit("SPN_OK") : sp_str_lit("SPN_ERROR");
  }

  sp_template_scope_t* scope = sp_template_scope_create(mem);
  sp_template_set(scope, sp_str_lit("mode"), mode);
  sp_template_set(scope, sp_str_lit("iter"), sp_fmt(mem, "{}", sp_fmt_uint(iter)).value);
  sp_template_set(scope, sp_str_lit("err"), verdict);
  sp_template_set(scope, sp_str_lit("linkage"), fz_linkage_name(u->profile.linkage));

  sp_template_list(scope, sp_str_lit("pkgs"));
  sp_da_for(u->pkgs, it) {
    sp_template_scope_t* pkg = sp_template_push(scope, sp_str_lit("pkgs"));
    sp_template_set(pkg, sp_str_lit("name"), fz_pkg_name((u32)it));

    sp_template_list(pkg, sp_str_lit("releases"));
    sp_da_for(u->pkgs[it].releases, rt) {
      fz_release_t* release = &u->pkgs[it].releases[rt];
      sp_template_scope_t* rel = sp_template_push(pkg, sp_str_lit("releases"));
      sp_template_set(rel, sp_str_lit("version"), sp_fmt(mem, "{}, {}, {}",
        sp_fmt_uint(release->version.major),
        sp_fmt_uint(release->version.minor),
        sp_fmt_uint(release->version.patch)).value);

      sp_template_list(rel, sp_str_lit("with_deps"));
      if (!sp_da_empty(release->deps)) {
        sp_template_scope_t* with = sp_template_push(rel, sp_str_lit("with_deps"));
        sp_template_list(with, sp_str_lit("deps"));
        sp_da_for(release->deps, dt) {
          sp_template_scope_t* dep = sp_template_push(with, sp_str_lit("deps"));
          sp_template_set(dep, sp_str_lit("name"), fz_pkg_name(release->deps[dt].pkg));
          sp_template_set(dep, sp_str_lit("range"), fz_range_render(mem, release->deps[dt]));
          sp_template_set(dep, sp_str_lit("attrs"), fz_dep_attrs(mem, release->deps[dt]));
        }
      }

      sp_template_list(rel, sp_str_lit("with_targets"));
      if (u->pkgs[it].linkages.source || u->pkgs[it].linkages.static_lib || u->pkgs[it].linkages.shared) {
        sp_template_scope_t* with = sp_template_push(rel, sp_str_lit("with_targets"));
        sp_template_set(with, sp_str_lit("target"), fz_pkg_name((u32)it));
        sp_template_set(with, sp_str_lit("linkages"), fz_render_linkages(mem, u->pkgs[it].linkages));
      }
    }
  }

  sp_template_list(scope, sp_str_lit("config"));
  sp_da_for(u->pkgs, it) {
    if (!u->pkgs[it].has_config) {
      continue;
    }
    sp_template_scope_t* entry = sp_template_push(scope, sp_str_lit("config"));
    sp_template_set(entry, sp_str_lit("name"), fz_pkg_name((u32)it));
    sp_template_set(entry, sp_str_lit("kind"), fz_linkage_name(u->pkgs[it].config));
  }

  sp_template_list(scope, sp_str_lit("roots"));
  sp_da_for(u->roots, it) {
    sp_template_scope_t* dep = sp_template_push(scope, sp_str_lit("roots"));
    sp_template_set(dep, sp_str_lit("name"), fz_pkg_name(u->roots[it].pkg));
    sp_template_set(dep, sp_str_lit("range"), fz_range_render(mem, u->roots[it]));
    sp_template_set(dep, sp_str_lit("attrs"), fz_root_attrs(mem, u->roots[it]));
  }

  sp_str_t tmpl = sp_zero;
  if (fz_load_template(mem, &tmpl)) {
    sp_template_render(&out.base, tmpl, scope, SP_NULLPTR);
  }
  else {
    sp_fmt_io(&out.base, "fuzz: repro template missing; replay with --iter {}\n", sp_fmt_uint(iter));
  }

  u64 widest = 0;
  bool locals = false;
  sp_da_for(u->pkgs, it) {
    locals = locals || u->pkgs[it].local;
    sp_da_for(u->pkgs[it].releases, rt) {
      widest = sp_max(widest, sp_da_size(u->pkgs[it].releases[rt].deps));
    }
  }
  if (widest > 4) {
    sp_fmt_io(&out.base, "fuzz: a release holds {} deps; fixture_t caps deps at 4, replay with --iter {}\n", sp_fmt_uint(widest), sp_fmt_uint(iter));
  }
  if (locals) {
    sp_fmt_io(&out.base, "fuzz: universe holds local packages; the fixture renders them as index entries, replay with --iter {}\n", sp_fmt_uint(iter));
  }

  sp_mem_end_scratch(scratch);
}
