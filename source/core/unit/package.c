#include "unit/package.h"

#include "core/core.h"
#include "ctx/ctx.h"
#include "sp.h"
#include "spn/core.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/git.h"
#include "external/tom.h"
#include "paths/paths.h"
#include "pkg/pkg.h"
#include "semver/convert.h"
#include "session/session.h"
#include "target/target.h"
#include "unit/types.h"
#include "unit/unit.h"

spn_path_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return spn_path_join(spn.mem, ctx->paths.stamp.dir, node->tag);
}

void spn_pkg_unit_create_layout(spn_pkg_unit_t* unit) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_path_t dirs [] = {
    unit->paths.work,
    unit->paths.include,
    unit->paths.lib,
    unit->paths.bin,
    unit->paths.vendor,
  };
  sp_carr_for(dirs, it) {
    sp_fs_create_dir(spn_path_str(&spn.roots, scratch.mem, dirs[it]));
  }
  sp_mem_end_scratch(scratch);
}

typedef sp_str_ht(sp_str_t) staged_header_set_t;

static spn_err_t header_collision(spn_pkg_unit_t* unit, sp_str_t path, sp_str_t first, sp_str_t second) {
  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_ERR,
    .pkg = unit->info->name,
    .err = {
      .kind = SPN_ERR_HEADER_COLLISION,
      .header_collision = {
        .path = path,
        .first = sp_str_copy(spn.mem, first),
        .second = sp_str_copy(spn.mem, second),
      },
    },
  });
  return SPN_ERROR;
}

static spn_err_t header_copy_failed(spn_pkg_unit_t* unit, sp_str_t path) {
  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_NODE_FAILED,
    .pkg = unit->info->name,
    .node_failed = {
      .path = path,
      .message = sp_str_lit("could not be published to the package store"),
    },
  });
  return SPN_ERROR;
}

typedef struct {
  sp_str_t from;
  sp_str_t to;
  sp_str_t name;
} staged_header_t;

static spn_err_t stage_target_headers(spn_pkg_unit_t* unit, sp_str_t root, spn_target_map_t targets, sp_mem_t mem, staged_header_set_t* seen, sp_da(staged_header_t)* staged) {
  const spn_path_roots_t* roots = &spn.roots;
  sp_om_for(targets, it) {
    spn_target_info_t* target = sp_str_om_at(targets, it);

    sp_da_for(target->headers, ht) {
      spn_path_t header = target->headers[ht];

      sp_str_t sub = spn_tree_rel(unit->paths.roots, header).sub;
      sp_str_t from = spn_path_str(roots, mem, header);
      sp_str_t to = sp_fs_join_path(mem, root, sub);

      sp_str_t* first = sp_str_ht_get(*seen, to);
      if (first) {
        if (!sp_str_equal(*first, from)) {
          return header_collision(unit, sub, *first, from);
        }
        continue;
      }
      sp_str_ht_insert(*seen, to, from);
      sp_da_push(*staged, ((staged_header_t) { .from = from, .to = to, .name = sub }));
    }
  }

  return SPN_OK;
}

static spn_err_t stage_headers(spn_pkg_unit_t* unit, sp_str_t root, sp_mem_t mem, sp_da(staged_header_t)* staged) {
  staged_header_set_t seen;
  sp_str_ht_init(mem, seen);

  spn_pkg_unit_header_maps_t published = spn_pkg_unit_header_maps(unit);
  sp_for(it, published.count) {
    spn_try(stage_target_headers(unit, root, published.maps[it], mem, &seen, staged));
  }
  return SPN_OK;
}

static spn_err_t copy_header(spn_pkg_unit_t* unit, const staged_header_t* header) {
  sp_fs_create_dir(sp_fs_parent_path(header->to));
  if (spn_fs_update_file(header->from, header->to)) {
    return header_copy_failed(unit, header->name);
  }
  return SPN_OK;
}

spn_err_t spn_pkg_unit_publish_headers(spn_pkg_unit_t* unit, sp_str_t root) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(staged_header_t) staged = sp_da_new(scratch.mem, staged_header_t);
  spn_err_t err = stage_headers(unit, root, scratch.mem, &staged);
  sp_da_for(staged, it) {
    if (err) {
      break;
    }
    err = copy_header(unit, &staged[it]);
  }
  sp_mem_end_scratch(scratch);
  return err;
}

spn_err_t spn_pkg_unit_publish_existing_headers(spn_pkg_unit_t* unit, sp_str_t root) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(staged_header_t) staged = sp_da_new(scratch.mem, staged_header_t);
  spn_err_t err = stage_headers(unit, root, scratch.mem, &staged);
  sp_da_for(staged, it) {
    if (err) {
      break;
    }
    if (!sp_fs_exists(staged[it].from)) {
      continue;
    }
    err = copy_header(unit, &staged[it]);
  }
  sp_mem_end_scratch(scratch);
  return err;
}

void spn_pkg_unit_write_stamp(spn_pkg_unit_t* unit, spn_path_t path) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_fs_create_file_str(spn_path_str(&spn.roots, s.mem, path), unit->info->name);
  sp_mem_end_scratch(s);
}

// @spader I think this is wrong; it's called in four places and deduplicated with an atomic,
// but really we just want to add one graph node to log before anything in a package is compiled.
// I think of of the existing nodes would even suffice for this.
void spn_pkg_unit_announce_compile(spn_pkg_unit_t* unit) {
  if (!sp_atomic_s32_cas(&unit->compile_announced, 0, 1, SP_ATOMIC_SEQ_CST)) return;

  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_COMPILE_START,
    .pkg = unit->info->name,
    .compile_start = {
      .version = spn_semver_to_str(spn.mem, unit->info->version),
    },
  });
}
