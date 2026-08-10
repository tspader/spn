#include "unit/package.h"

#include "ctx/ctx.h"
#include "error/types.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/git.h"
#include "external/tom.h"
#include "pkg/pkg.h"
#include "session/session.h"
#include "target/target.h"
#include "unit/types.h"

sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return sp_fs_join_path(spn.mem, ctx->paths.stamp.dir, node->tag);
}

void spn_pkg_unit_create_layout(spn_pkg_unit_t* unit) {
  sp_fs_create_dir(unit->paths.work);
  sp_fs_create_dir(unit->paths.include);
  sp_fs_create_dir(unit->paths.lib);
  sp_fs_create_dir(unit->paths.bin);
  sp_fs_create_dir(unit->paths.vendor);
}

typedef sp_str_ht(sp_str_t) staged_header_set_t;

sp_str_t spn_pkg_unit_header_source(sp_mem_t mem, spn_pkg_unit_t* unit, spn_tree_path_t header) {
  if (sp_fs_is_absolute(header.path)) {
    return header.path;
  }
  sp_str_t root = header.tree == SPN_TREE_MANIFEST ? unit->paths.recipe : unit->paths.source;
  return sp_fs_join_path(mem, root, header.path);
}

static spn_err_t header_collision(spn_pkg_unit_t* unit, sp_str_t path) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_NODE_FAILED,
    .pkg = unit->info,
    .io = &unit->logs.io,
    .node_failed = {
      .path = path,
      .message = sp_str_lit("is published from both trees; the destination would collide"),
    },
  });
  return SPN_ERROR;
}

static spn_err_t publish_target_headers(spn_pkg_unit_t* unit, spn_target_map_t targets, bool strict, sp_mem_t mem, staged_header_set_t* staged) {
  sp_om_for(targets, it) {
    spn_target_info_t* target = sp_str_om_at(targets, it);

    sp_da_for(target->headers, h) {
      spn_tree_path_t header = target->headers[h];

      sp_str_t from = spn_pkg_unit_header_source(mem, unit, header);
      sp_str_t to = sp_fs_join_path(mem, unit->paths.include, header.path);

      sp_str_t* seen = sp_str_ht_get(*staged, to);
      if (seen) {
        if (!sp_str_equal(*seen, from)) {
          return header_collision(unit, header.path);
        }
        continue;
      }
      sp_str_ht_insert(*staged, to, from);

      if (strict || sp_fs_exists(from)) {
        sp_fs_create_dir(sp_fs_parent_path(to));
        spn_try(sp_fs_copy(from, to) ? SPN_ERROR : SPN_OK);
      }
    }
  }

  return SPN_OK;
}

spn_err_t spn_pkg_unit_publish_headers(spn_pkg_unit_t* unit, bool strict) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  staged_header_set_t staged;
  sp_str_ht_init(scratch.mem, staged);

  spn_err_t err = publish_target_headers(unit, unit->info->libs, strict, scratch.mem, &staged);
  if (!err && unit->source == SPN_PKG_SOURCE_ROOT) {
    spn_target_map_t maps [] = { unit->info->exes, unit->info->scripts, unit->info->tests };
    sp_carr_for(maps, it) {
      err = publish_target_headers(unit, maps[it], strict, scratch.mem, &staged);
      if (err) {
        break;
      }
    }
  }

  sp_mem_end_scratch(scratch);
  return err;
}

void spn_pkg_unit_write_stamp(spn_pkg_unit_t* unit, sp_str_t path) {
  sp_fs_create_file_str(path, unit->info->name);
}

// @spader I think this is wrong; it's called in four places and deduplicated with an atomic,
// but really we just want to add one graph node to log before anything in a package is compiled.
// I think of of the existing nodes would even suffice for this.
void spn_pkg_unit_announce_compile(spn_pkg_unit_t* unit) {
  if (!sp_atomic_s32_cas(&unit->compile_announced, 0, 1, SP_ATOMIC_SEQ_CST)) return;

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_COMPILE_START,
    .pkg = unit->info,
    .io = &unit->logs.io,
  });
}
