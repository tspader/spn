#include "unit/package.h"

#include "core/core.h"
#include "ctx/ctx.h"
#include "error/types.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/git.h"
#include "external/tom.h"
#include "pkg/pkg.h"
#include "semver/convert.h"
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

static spn_err_t header_collision(spn_pkg_unit_t* unit, sp_str_t path, sp_str_t first, sp_str_t second) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
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
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_NODE_FAILED,
    .pkg = unit->info->name,
    .node_failed = {
      .path = path,
      .message = sp_str_lit("could not be published to the package store"),
    },
  });
  return SPN_ERROR;
}

static spn_err_t publish_target_headers(spn_pkg_unit_t* unit, sp_str_t root, spn_target_map_t targets, spn_publish_t publish, sp_mem_t mem, staged_header_set_t* staged) {
  sp_om_for(targets, it) {
    spn_target_info_t* target = sp_str_om_at(targets, it);

    sp_da_for(target->headers, ht) {
      spn_tree_path_t header = target->headers[ht];

      sp_str_t from = spn_tree_path_resolve(mem, unit->paths.roots, header);
      sp_str_t to = sp_fs_join_path(mem, root, header.path);

      sp_str_t* seen = sp_str_ht_get(*staged, to);
      if (seen) {
        if (!sp_str_equal(*seen, from)) {
          return header_collision(unit, header.path, *seen, from);
        }
        continue;
      }
      sp_str_ht_insert(*staged, to, from);

      if (publish == SPN_PUBLISH_EXISTING && !sp_fs_exists(from)) {
        continue;
      }
      sp_fs_create_dir(sp_fs_parent_path(to));
      if (sp_fs_copy(from, to)) {
        return header_copy_failed(unit, header.path);
      }
    }
  }

  return SPN_OK;
}

spn_err_t spn_pkg_unit_publish_headers(spn_pkg_unit_t* unit, sp_str_t root, spn_publish_t publish) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  staged_header_set_t staged;
  sp_str_ht_init(scratch.mem, staged);

  spn_err_t err = SPN_OK;
  spn_pkg_unit_header_maps_t published = spn_pkg_unit_header_maps(unit);
  sp_for(it, published.count) {
    err = publish_target_headers(unit, root, published.maps[it], publish, scratch.mem, &staged);
    if (err) {
      break;
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
    .pkg = unit->info->name,
    .compile_start = {
      .version = spn_semver_to_str(spn.mem, unit->info->version),
    },
  });
}
