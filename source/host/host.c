#include "spn/host.h"

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "index/index.h"
#include "index/types.h"
#include "op/build/build.h"
#include "pkg/pkg.h"
#include "pkg/types.h"
#include "project/types.h"
#include "session/session.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

static spn_index_desc_t describe_index(spn_index_info_t* index) {
  return (spn_index_desc_t) {
    .name = index->name,
    .kind = index->kind,
    .protocol = index->protocol,
    .source = spn_index_source(index),
    .location = index->location,
  };
}

bool spn_ctx_find_target(spn_ctx_t* ctx, sp_str_t name, spn_target_kind_t* kind) {
  if (!ctx->project) {
    return false;
  }
  spn_target_info_t* info = spn_pkg_get_target_ex(&ctx->project->package, name);
  if (!info) {
    return false;
  }
  *kind = info->kind;
  return true;
}

sp_str_t spn_ctx_project_dir(spn_ctx_t* ctx) {
  return ctx->paths.project;
}

sp_str_t spn_ctx_cache_dir(spn_ctx_t* ctx) {
  return ctx->paths.caches.dir;
}

spn_index_arr_t spn_get_indexes(sp_mem_t mem, spn_ctx_t* ctx) {
  spn_index_arr_t indexes = {
    .items = sp_alloc_n(mem, spn_index_desc_t, sp_da_size(ctx->indexes)),
    .count = (u32)sp_da_size(ctx->indexes),
  };
  sp_da_for(ctx->indexes, it) {
    spn_index_desc_t desc = describe_index(&ctx->indexes[it]);
    desc.name = sp_str_copy(mem, desc.name);
    desc.source = sp_str_copy(mem, desc.source);
    desc.location = sp_str_copy(mem, desc.location);
    indexes.items[it] = desc;
  }
  return indexes;
}

bool spn_get_index(spn_ctx_t* ctx, sp_str_t name, spn_index_desc_t* index) {
  spn_index_info_t* info = spn_find_index(ctx, name);
  if (!info) {
    return false;
  }
  *index = describe_index(info);
  return true;
}

spn_target_t* spn_session_find_target(spn_session_t* session, sp_str_t name) {
  spn_pkg_unit_t* root = spn_session_find_pkg_unit(session, session->units.target, spn_session_root_pkg(session));
  sp_assert(root);
  return spn_session_find_target_in_pkg(session, root, name);
}

sp_str_t spn_target_name(spn_target_t* target) {
  return target->info->name;
}

sp_str_t spn_target_path(sp_mem_t mem, spn_target_t* target) {
  return spn_target_unit_staged_path(mem, target);
}
