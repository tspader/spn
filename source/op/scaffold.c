#include "spn/host.h"

#include "ctx/types.h"
#include "error/error.h"
#include "op/op.h"
#include "sp/sp_template.h"
#include "spn.embed.h"

typedef struct {
  u32 index;
  bool bare;
  bool done;
  sp_str_t rel;
  sp_str_t tpl;
} iterator_t;

static void it_next(iterator_t* it) {
  while (it->index < sp_carr_len(spn_embed_manifest)) {
    spn_embed_entry_t entry = spn_embed_manifest[it->index++];
    sp_str_t path = sp_cstr_as_str(entry.path);
    if (!sp_str_starts_with(path, sp_str_lit("init/"))) {
      continue;
    }

    sp_str_t rel = sp_str_strip_left(path, sp_str_lit("init/"));
    if (it->bare && !sp_str_equal_cstr(rel, "spn.toml")) {
      continue;
    }
    if (sp_str_equal_cstr(rel, "gitignore")) {
      rel = sp_str_lit(".gitignore");
    }

    it->rel = rel;
    it->tpl = sp_str((const c8*)entry.data, entry.size);
    return;
  }

  it->done = true;
}

static iterator_t it_new(bool bare) {
  iterator_t it = { .bare = bare };
  it_next(&it);
  return it;
}

static bool is_name_valid(sp_str_t name) {
  if (sp_str_empty(name)) {
    return false;
  }
  sp_str_for(name, it) {
    c8 c = name.data[it];
    if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      return false;
    }
  }
  return true;
}

static spn_err_union_t scaffold(spn_ctx_t* ctx, spn_scaffold_request_t request, sp_mem_t mem, spn_str_arr_t* files) {
  *files = sp_zero_s(spn_str_arr_t);
  if (!is_name_valid(request.name)) {
    return (spn_err_union_t) { .kind = SPN_ERR_INIT_NAME, .pkg = { .name = sp_str_copy(ctx->heap, request.name) } };
  }
  if (sp_fs_create_dir(request.dir) != SP_OK) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(ctx->heap, request.dir) } };
  }

  sp_da(sp_str_t) created = sp_da_new(mem, sp_str_t);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_union_t err = spn_result(SPN_OK);

  sp_template_scope_t* scope = sp_template_scope_create(s.mem);
  sp_template_set(scope, sp_str_lit("name"), request.name);

  for (iterator_t it = it_new(request.bare); !it.done; it_next(&it)) {
    sp_str_t path = sp_fs_join_path(s.mem, request.dir, it.rel);

    sp_io_dyn_mem_writer_t writer = sp_zero;
    sp_io_dyn_mem_writer_init(s.mem, &writer);
    if (sp_template_render(&writer.base, it.tpl, scope, SP_NULLPTR)) {
      err = spn_result(SPN_ERROR);
      break;
    }
    if (sp_fs_create_file_str(path, sp_io_dyn_mem_writer_take_str(&writer)) != SP_OK) {
      err = (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(ctx->heap, path) } };
      break;
    }

    sp_da_push(created, sp_str_copy(mem, it.rel));
  }

  *files = (spn_str_arr_t) { .items = created, .count = (u32)sp_da_size(created) };
  sp_mem_end_scratch(s);
  return err;
}

static spn_err_union_t check(spn_ctx_t* ctx, spn_scaffold_request_t request) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_union_t err = spn_result(SPN_OK);

  for (iterator_t it = it_new(request.bare); !it.done; it_next(&it)) {
    sp_str_t path = sp_fs_join_path(s.mem, request.dir, it.rel);
    if (sp_fs_exists(path)) {
      err = (spn_err_union_t) { .kind = SPN_ERR_INIT_EXISTS, .fs = { .path = sp_str_copy(ctx->heap, path) } };
      break;
    }
  }

  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_op_scaffold(spn_op_t* op) {
  return spn_err_emit(op->ctx, scaffold(op->ctx, op->request.scaffold, op->mem, &op->result.scaffold.files));
}

spn_op_t* spn_scaffold_project(spn_ctx_t* ctx, spn_scaffold_request_t request) {
  spn_op_t* op = spn_op_new(ctx, SP_NULLPTR, SPN_OP_SCAFFOLD);
  op->request.scaffold = (spn_scaffold_request_t) {
    .dir = sp_str_copy(ctx->heap, request.dir),
    .name = sp_str_copy(ctx->heap, request.name),
    .bare = request.bare,
  };
  spn_op_submit(op);
  return op;
}

spn_err_t spn_scaffold_check(spn_ctx_t* ctx, spn_scaffold_request_t request) {
  return spn_err_emit(ctx, check(ctx, request));
}
