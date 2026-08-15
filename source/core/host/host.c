#include "spn/host.h"

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "index/index.h"
#include "index/types.h"
#include "paths/paths.h"

static spn_index_desc_t describe_index(spn_index_info_t* index) {
  return (spn_index_desc_t) {
    .name = index->name,
    .kind = index->kind,
    .protocol = index->protocol,
    .source = spn_index_source(index),
    .location = index->location,
  };
}

sp_str_t spn_ctx_project_dir(spn_ctx_t* ctx) {
  return ctx->paths.project;
}

sp_str_t spn_ctx_cache_dir(spn_ctx_t* ctx) {
  return ctx->paths.caches.dir;
}

spn_path_t spn_ctx_collapse_path(spn_ctx_t* ctx, sp_str_t path) {
  if (!sp_fs_is_absolute(path) || !spn_path_normal(path)) {
    return (spn_path_t) { .sub = path };
  }
  return spn_path_make(&ctx->roots, path);
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
