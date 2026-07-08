#include "app/types.h"
#include "cli/cli.h"

#include "cli/types.h"
#include "ctx/types.h"
#include "index/cache.h"
#include "index/types.h"
#include "log/log.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "sp/atomic_file.h"
#include "sp/sp_cli.h"
#include "toml/edit.h"

typedef struct {
  sp_str_t path [4];
  u32 num_segments;
} spn_add_site_t;

static spn_add_site_t spn_add_find_site(spn_toml_edit_t* edit, sp_mem_t mem, sp_str_t table, sp_str_t name, sp_str_t qualified) {
  sp_str_t deps [2] = { sp_str_lit("deps"), table };
  sp_da(sp_str_t) keys = spn_toml_edit_keys(edit, mem, deps, 2);

  sp_da_for(keys, it) {
    if (!sp_str_equal(spn_pkg_canonicalize_name(keys[it]), qualified)) {
      continue;
    }

    spn_add_site_t site = {
      .path = { sp_str_lit("deps"), table, keys[it], sp_str_lit("version") },
      .num_segments = 4,
    };

    spn_toml_edit_entry_t* entry = spn_toml_edit_find(edit, site.path, 3);
    if (entry && entry->kind != SPN_TOML_EDIT_VALUE_TABLE) {
      site.num_segments = 3;
    }
    return site;
  }

  return (spn_add_site_t) {
    .path = { sp_str_lit("deps"), table, name },
    .num_segments = 3,
  };
}

static sp_str_t spn_add_run(spn_cli_add_t* command, sp_mem_arena_marker_t s) {
  if (command->test && command->build) {
    return sp_fmt(s.mem, "pass at most one of {.cyan} and {.cyan}", sp_fmt_cstr("--test"), sp_fmt_cstr("--build")).value;
  }

  sp_str_pair_t request = sp_str_cleave_c8(command->package, '@');
  sp_str_t name = request.first;
  sp_str_t requested = request.second;

  if (sp_str_empty(name)) {
    return sp_str_lit("expected a package name");
  }

  spn_index_cache_t cache = sp_zero;
  spn_index_cache_init(&cache, spn.heap, spn.intern, &spn.indexes);

  spn_pkg_name_t id = spn_pkg_name_from_qualified(name);
  sp_str_t qualified= spn_pkg_name_to_qualified(id);
  spn_index_pkg_t* pkg = spn_index_cache_get_package(&cache, id);
  if (!pkg || sp_da_empty(pkg->releases)) {
    return sp_fmt(s.mem, "package {.cyan} not found in any index", sp_fmt_str(name)).value;
  }

  spn_semver_range_t range = spn_semver_any();
  if (!sp_str_empty(requested)) {
    if (spn_semver_parse_range(requested, &range)) {
      return sp_fmt(s.mem, "invalid version {.red}", sp_fmt_str(requested)).value;
    }
  }

  spn_index_rel_t* release = SP_NULLPTR;
  sp_da_rfor(pkg->releases, it) {
    spn_index_rel_t* candidate = &pkg->releases[it];
    if (candidate->yanked) {
      continue;
    }
    if (!spn_semver_in_range(candidate->version, range)) {
      continue;
    }
    release = candidate;
    break;
  }

  if (!release) {
    sp_str_t shown = sp_str_empty(requested) ? sp_str_lit("*") : requested;
    return sp_fmt(s.mem, "no version of {.cyan} matches {.red}", sp_fmt_str(name), sp_fmt_str(shown)).value;
  }

  sp_str_t version = requested;
  if (sp_str_empty(version)) {
    version = spn_semver_to_str(s.mem, release->version);
  }

  sp_str_t source = sp_zero;
  if (sp_io_read_file(s.mem, spn.paths.manifest, &source) != SP_OK) {
    return sp_fmt(s.mem, "failed to read {.cyan}", sp_fmt_str(spn.paths.manifest)).value;
  }

  spn_toml_edit_t edit = sp_zero;
  if (spn_toml_edit_init(&edit, s.mem, source)) {
    return sp_fmt(s.mem, "failed to parse {.cyan}", sp_fmt_str(spn.paths.manifest)).value;
  }

  const c8* table = SP_NULLPTR;
  if (command->test) {
    table = "test";
  }
  else if (command->build) {
    table = "build";
  }
  else {
    table = "package";
  }

  spn_add_site_t site = spn_add_find_site(&edit, s.mem, sp_cstr_as_str(table), name, qualified);
  if (spn_toml_edit_set_str(&edit, site.path, site.num_segments, version)) {
    return sp_fmt(s.mem, "failed to edit {.cyan}", sp_fmt_str(spn.paths.manifest)).value;
  }

  sp_str_t updated = spn_toml_edit_render(&edit, s.mem);
  if (sp_fs_write_atomic(spn.paths.manifest, updated) != SP_OK) {
    return sp_fmt(s.mem, "failed to write {.cyan}", sp_fmt_str(spn.paths.manifest)).value;
  }

  spn_log_info("Added {.cyan}=={.green}", sp_fmt_str(site.path[2]), sp_fmt_str(version));
  return sp_zero_s(sp_str_t);
}

spn_task_result_t spn_task_add(spn_app_t* app) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_str_t error = spn_add_run(&spn.cli.add, s);
  if (!sp_str_empty(error)) {
    spn_log_error("{}", sp_fmt_str(error));
  }
  sp_mem_end_scratch(s);
  return sp_str_empty(error) ? SPN_TASK_DONE : SPN_TASK_ERROR;
}

sp_cli_result_t spn_cli_add(sp_cli_t* cli) {
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_ADD);
  return SP_CLI_CONTINUE;
}
