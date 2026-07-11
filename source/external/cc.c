#include "sp.h"
#include "sp/macro.h"
#include "external/cc.h"
#include "sp/io.h"

sp_str_t spn_cc_symbol_from_embedded_file(sp_mem_t mem, sp_str_t file_path) {
  c8* data = sp_alloc_n(mem, c8, file_path.len);
  for (u32 it = 0; it < file_path.len; it++) {
    c8 c = file_path.data[it];
    data[it] = (c == '/' || c == '.' || c == '-') ? '_' : c;
  }
  return sp_str(data, file_path.len);
}

void spn_cc_embed_ctx_init(spn_cc_embed_ctx_t* c, sp_mem_t mem, spn_os_t os, spn_arch_t arch) {
  c->arena = sp_mem_arena_new(mem);
  c->mem = sp_mem_arena_as_allocator(c->arena);
  sp_da_init(c->mem, c->entries);

  spn_obj_kind_t format;
  switch (os) {
    case SPN_OS_WINDOWS: format = SPN_OBJ_COFF; break;
    case SPN_OS_MACOS: format = SPN_OBJ_MACHO; break;
    case SPN_OS_LINUX:
    case SPN_OS_WASI: format = SPN_OBJ_ELF; break;
    case SPN_OS_NONE: sp_unreachable_case();
  }
  spn_obj_init(&c->obj, c->mem, format, arch);
}

void spn_cc_embed_ctx_free(spn_cc_embed_ctx_t* ctx) {
  sp_mem_arena_destroy(ctx->arena);
  *ctx = sp_zero_s(spn_cc_embed_ctx_t);
}

spn_err_t spn_cc_embed_ctx_add(
  spn_cc_embed_ctx_t* ctx,
  sp_mem_buffer_t data,
  sp_str_t symbol,
  sp_str_t path,
  sp_str_t data_type,
  sp_str_t size_type
) {
  symbol = sp_str_copy(ctx->mem, symbol);

  spn_obj_add_symbol(&ctx->obj, symbol, data.data, data.len);
  spn_obj_add_symbol(&ctx->obj, sp_fmt(ctx->mem, "{}_size", sp_fmt_str(symbol)).value, &data.len, sizeof(u64));

  sp_da_push(ctx->entries, ((spn_cc_embed_t) {
    .path = sp_str_copy(ctx->mem, path),
    .symbol = symbol,
    .size = data.len,
    .types = {
      .size = sp_str_copy(ctx->mem, size_type),
      .data = sp_str_copy(ctx->mem, data_type),
    }
  }));

  return SPN_OK;
}

spn_err_t spn_cc_embed_ctx_write(spn_cc_embed_ctx_t* ctx, sp_str_t object, sp_str_t header) {
  spn_try_as(spn_obj_write(&ctx->obj, object), SPN_ERROR);

  sp_io_file_writer_t writer = sp_zero;
  spn_try_as(sp_io_file_writer_from_path(&writer, header), SPN_ERROR);
  sp_io_writer_t* io = &writer.base;
  sp_da_for(ctx->entries, it) {
    spn_cc_embed_t entry = ctx->entries[it];
    sp_fmt_io(io,
      "extern const {} {} [{}];",
      SP_FMT_STR(entry.types.data),
      SP_FMT_STR(entry.symbol),
      SP_FMT_U64(entry.size)
    );
    sp_io_write_new_line(io);

    sp_fmt_io(io,
      "extern const {} {}_size;",
      SP_FMT_STR(entry.types.size),
      SP_FMT_STR(entry.symbol)
    );
    sp_io_write_new_line(io);
    sp_io_write_new_line(io);
  }

  sp_io_write_str(io, sp_str_lit("typedef struct {"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("  const char* path;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("  const void* data;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("  unsigned long long size;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("} spn_embed_entry_t;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_fmt_io(io, "static const unsigned int spn_embed_count = {};", SP_FMT_U32(sp_da_size(ctx->entries)));
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("static const spn_embed_entry_t spn_embed_manifest[] = {"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_da_for(ctx->entries, it) {
    spn_cc_embed_t entry = ctx->entries[it];
    sp_io_write_cstr(io, "  { ", SP_NULLPTR);
    sp_fmt_io(io, "\"{}\", {}, {}", SP_FMT_STR(entry.path), SP_FMT_STR(entry.symbol), SP_FMT_U64(entry.size));
    sp_io_write_cstr(io, " },", SP_NULLPTR);
    sp_io_write_new_line(io);
  }
  sp_io_write_str(io, sp_str_lit("};"), SP_NULLPTR);
  sp_io_write_new_line(io);

  sp_io_file_writer_close(&writer);
  return SPN_OK;
}
