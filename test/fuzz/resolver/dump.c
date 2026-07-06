#include "fuzz.h"

#define SP_TEMPLATE_IMPLEMENTATION
#include "sp_template.h"

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

void fz_dump(fz_universe_t* u, u64 iter) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_mem_t mem = scratch.mem;

  sp_template_scope_t* scope = sp_template_scope_create(mem);
  sp_template_set(scope, sp_str_lit("mode"), u->planted ? sp_str_lit("planted") : sp_str_lit("free"));
  sp_template_set(scope, sp_str_lit("iter"), sp_fmt(mem, "{}", sp_fmt_uint(iter)).value);
  sp_template_set(scope, sp_str_lit("err"), u->planted ? sp_str_lit("SPN_OK") : sp_str_lit("<oracle: SAT>"));

  sp_template_list(scope, sp_str_lit("pkgs"));
  sp_da_for(u->pkgs, it) {
    sp_template_scope_t* pkg = sp_template_push(scope, sp_str_lit("pkgs"));
    sp_template_set(pkg, sp_str_lit("name"), sp_str_view(fz_names[it]));

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
          sp_template_set(dep, sp_str_lit("name"), sp_str_view(fz_names[release->deps[dt].pkg]));
          sp_template_set(dep, sp_str_lit("range"), fz_range_render(mem, release->deps[dt]));
        }
      }
    }
  }

  sp_template_list(scope, sp_str_lit("roots"));
  sp_da_for(u->roots, it) {
    sp_template_scope_t* dep = sp_template_push(scope, sp_str_lit("roots"));
    sp_template_set(dep, sp_str_lit("name"), sp_str_view(fz_names[u->roots[it].pkg]));
    sp_template_set(dep, sp_str_lit("range"), fz_range_render(mem, u->roots[it]));
  }

  sp_io_stream_writer_t out = sp_io_get_std_out();
  sp_str_t tmpl = sp_zero;
  if (fz_load_template(mem, &tmpl)) {
    sp_template_render(&out.base, tmpl, scope, SP_NULLPTR);
  }
  else {
    sp_fmt_io(&out.base, "fuzz: repro template missing; replay with SPN_FUZZ_ITER={}\n", sp_fmt_uint(iter));
  }

  sp_mem_end_scratch(scratch);
}
