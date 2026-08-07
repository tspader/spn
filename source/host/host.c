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

static spn_target_unit_t* unwrap(spn_target_t* target) {
  return sp_ptr_cast(spn_target_unit_t*, target);
}

static spn_target_t* wrap(spn_target_unit_t* unit) {
  return sp_ptr_cast(spn_target_t*, unit);
}

static spn_index_desc_t describe_index(spn_index_info_t* index) {
  return (spn_index_desc_t) {
    .name = index->name,
    .kind = index->kind,
    .protocol = index->protocol,
    .source = spn_index_source(index),
    .location = index->location,
  };
}

bool spn_ctx_has_project(spn_ctx_t* ctx) {
  return ctx->project != SP_NULLPTR;
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

sp_da(spn_index_desc_t) spn_ctx_indexes(sp_mem_t mem, spn_ctx_t* ctx) {
  sp_da(spn_index_desc_t) indexes = sp_da_new(mem, spn_index_desc_t);
  sp_da_for(ctx->indexes, it) {
    spn_index_desc_t desc = describe_index(&ctx->indexes[it]);
    desc.name = sp_str_copy(mem, desc.name);
    desc.source = sp_str_copy(mem, desc.source);
    desc.location = sp_str_copy(mem, desc.location);
    sp_da_push(indexes, desc);
  }
  return indexes;
}

bool spn_ctx_find_index(spn_ctx_t* ctx, sp_str_t name, spn_index_desc_t* index) {
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
  spn_target_unit_t* unit = spn_session_find_target_in_pkg(session, root, name);
  return unit ? wrap(unit) : SP_NULLPTR;
}

sp_str_t spn_target_name(spn_target_t* target) {
  return unwrap(target)->info->name;
}

sp_str_t spn_target_path(sp_mem_t mem, spn_target_t* target) {
  return spn_target_unit_staged_path(mem, unwrap(target));
}
