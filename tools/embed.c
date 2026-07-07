#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/sp_elf.h"
#include "sp/coff.h"

typedef enum {
  EMBED_FORMAT_ELF,
  EMBED_FORMAT_COFF,
} embed_format_t;

typedef struct {
  const c8* output;
  const c8* header;
  const c8* format;
} embed_t;

typedef struct {
  embed_format_t format;
  struct {
    sp_elf_t* elf;
    u32 rodata;
  } elf;
  struct {
    sp_coff_t* coff;
    sp_coff_section_t* rdata;
  } coff;
} embed_obj_t;

static embed_obj_t embed_obj_new(sp_mem_t mem, embed_format_t format) {
  embed_obj_t obj = { .format = format };
  switch (format) {
    case EMBED_FORMAT_ELF: {
      obj.elf.elf = sp_elf_new(mem);
      obj.elf.rodata = sp_elf_add_section(obj.elf.elf, (sp_elf_section_t){
        .name = sp_str_lit(".rodata"),
        .type = SHT_PROGBITS,
        .flags = SHF_ALLOC,
        .align = 8,
      });
      break;
    }
    case EMBED_FORMAT_COFF: {
      obj.coff.coff = sp_coff_new(mem);
      obj.coff.rdata = sp_coff_add_section(
        obj.coff.coff,
        sp_str_lit(".rdata"),
        SP_COFF_SCN_CNT_INITIALIZED_DATA | SP_COFF_SCN_MEM_READ | SP_COFF_SCN_ALIGN_8BYTES);
      break;
    }
  }
  return obj;
}

static void embed_obj_add(embed_obj_t* obj, sp_str_t symbol, const void* data, u64 size) {
  switch (obj->format) {
    case EMBED_FORMAT_ELF: {
      u64 offset = sp_elf_append_aligned(obj->elf.elf, obj->elf.rodata, data, size, 8);
      sp_elf_add_symbol(obj->elf.elf, (sp_elf_symbol_t){
        .name = symbol,
        .section = obj->elf.rodata,
        .value = offset,
        .size = size,
        .bind = STB_GLOBAL,
        .type = STT_OBJECT,
      });
      break;
    }
    case EMBED_FORMAT_COFF: {
      sp_io_writer_t* w = &obj->coff.rdata->writer.base;
      u64 head = obj->coff.rdata->writer.storage.len;
      u64 offset = sp_align_offset(head, 8);
      if (offset > head) sp_io_pad(w, offset - head, SP_NULLPTR);
      sp_io_write(w, data, size, SP_NULLPTR);
      sp_coff_add_symbol(obj->coff.coff, symbol, (u32)offset, 1, SP_COFF_SYM_CLASS_EXTERNAL);
      break;
    }
  }
}

static sp_err_t embed_obj_write(embed_obj_t* obj, sp_io_writer_t* out) {
  switch (obj->format) {
    case EMBED_FORMAT_ELF:  return sp_elf_write(obj->elf.elf, out);
    case EMBED_FORMAT_COFF: return sp_coff_write(obj->coff.coff, out);
  }
  return SP_ERR;
}

static void embed_obj_free(embed_obj_t* obj) {
  switch (obj->format) {
    case EMBED_FORMAT_ELF:  sp_elf_free(obj->elf.elf); break;
    case EMBED_FORMAT_COFF: sp_coff_free(obj->coff.coff); break;
  }
}

typedef struct {
  sp_str_t path;
  sp_str_t symbol;
  u64 size;
} embed_entry_t;

static sp_str_t embed_symbol_from_path(sp_mem_t mem, sp_str_t path) {
  sp_str_t symbol = sp_str_replace_c8(mem, path, '/', '_');
  symbol = sp_str_replace_c8(mem, symbol, '\\', '_');
  symbol = sp_str_replace_c8(mem, symbol, '.', '_');
  symbol = sp_str_replace_c8(mem, symbol, '-', '_');
  return symbol;
}

static sp_str_t embed_header_path(sp_mem_t mem, sp_str_t path) {
  sp_str_t ext = sp_fs_get_ext(path);
  if (!sp_str_empty(ext)) {
    path = sp_str_sub(path, 0, path.len - ext.len - 1);
  }
  return sp_fmt(mem, "{}.h", sp_fmt_str(path)).value;
}

static sp_cli_result_t embed_one(
  sp_cli_t* cli,
  sp_mem_t mem,
  embed_obj_t* obj,
  sp_da(embed_entry_t)* entries,
  sp_str_t src_path,
  sp_str_t embed_path
) {
  sp_str_t content = sp_zero;
  if (sp_io_read_file(mem, src_path, &content) != SP_OK) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to read {.red}", sp_fmt_str(src_path)).value);
  }

  sp_str_t symbol = embed_symbol_from_path(mem, embed_path);
  u64 size = content.len;

  embed_obj_add(obj, symbol, content.data, size);
  embed_obj_add(obj, sp_fmt(mem, "{}_size", sp_fmt_str(symbol)).value, &size, sizeof(u64));

  sp_da_push(*entries, ((embed_entry_t){
    .path = embed_path,
    .symbol = symbol,
    .size = size,
  }));

  return SP_CLI_OK;
}

sp_cli_result_t embed_run(sp_cli_t* cli) {
  embed_t* embed = sp_cast(embed_t*, cli->user_data);

  if (!cli->num_rest) {
    return sp_cli_set_error_c(cli, "no input files");
  }

  sp_mem_t mem = sp_mem_os_new();
  sp_str_t obj_path = sp_str_view(embed->output);
  sp_str_t hdr_path = embed->header ? sp_str_view(embed->header) : embed_header_path(mem, obj_path);

  embed_format_t format = EMBED_FORMAT_ELF;
  if (embed->format) {
    sp_str_t name = sp_str_view(embed->format);
    if (sp_str_equal(name, sp_str_lit("elf"))) {
      format = EMBED_FORMAT_ELF;
    }
    else if (sp_str_equal(name, sp_str_lit("coff"))) {
      format = EMBED_FORMAT_COFF;
    }
    else {
      return sp_cli_set_error(cli, sp_fmt(mem, "unknown format {.red}", sp_fmt_str(name)).value);
    }
  }

  embed_obj_t obj = embed_obj_new(mem, format);

  sp_da(embed_entry_t) entries = SP_NULLPTR;
  sp_da_init(mem, entries);

  sp_for(it, cli->num_rest) {
    sp_str_t arg = sp_str_view(cli->rest[it]);

    sp_str_t src = arg;
    sp_str_t dest = sp_zero;
    s32 eq = sp_str_find_c8(arg, '=');
    if (eq >= 0) {
      src = sp_str_sub(arg, 0, eq);
      dest = sp_str_sub(arg, eq + 1, arg.len - eq - 1);
    }

    if (sp_fs_is_dir(src)) {
      sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(mem, src);
      u32 skip = src.len + 1;
      sp_da_for(files, fit) {
        sp_fs_entry_t ent = files[fit];
        if (ent.kind == SP_FS_KIND_DIR) continue;
        sp_str_t rel = sp_str_sub(ent.path, skip, ent.path.len - skip);
        sp_str_t embed_path = sp_str_empty(dest) ? rel : sp_fs_join_path(mem, dest, rel);
        if (embed_one(cli, mem, &obj, &entries, ent.path, embed_path) != SP_CLI_OK) {
          return SP_CLI_ERR;
        }
      }
    }
    else {
      sp_str_t embed_path = sp_str_empty(dest) ? sp_fs_get_name(src) : dest;
      if (embed_one(cli, mem, &obj, &entries, src, embed_path) != SP_CLI_OK) {
        return SP_CLI_ERR;
      }
    }
  }

  sp_io_dyn_mem_writer_t header;
  sp_io_dyn_mem_writer_init(mem, &header);
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_fmt_io(&header.base, "extern const unsigned char {}[{}];\n", sp_fmt_str(entry.symbol), sp_fmt_uint(entry.size));
    sp_fmt_io(&header.base, "extern const unsigned long long {}_size;\n", sp_fmt_str(entry.symbol));
  }
  sp_fmt_io(&header.base, "\ntypedef struct {{\n  const char* path;\n  const void* data;\n  unsigned long long size;\n}} spn_embed_entry_t;\n");
  sp_fmt_io(&header.base, "static const unsigned int spn_embed_count = {};\n", sp_fmt_uint(sp_da_size(entries)));
  sp_fmt_io(&header.base, "static const spn_embed_entry_t spn_embed_manifest[] = {{\n");
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_fmt_io(&header.base, "  {{ \"{}\", {}, {} }},\n", sp_fmt_str(entry.path), sp_fmt_str(entry.symbol), sp_fmt_uint(entry.size));
  }
  sp_fmt_io(&header.base, "}};\n");

  sp_io_dyn_mem_writer_t object;
  sp_io_dyn_mem_writer_init(mem, &object);
  if (embed_obj_write(&obj, &object.base) != SP_OK) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to encode {.red}", sp_fmt_str(obj_path)).value);
  }

  sp_str_t obj_dir = sp_fs_parent_path(obj_path);
  if (!sp_str_empty(obj_dir) && sp_fs_create_dir(obj_dir) != SP_OK) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to create {.red}", sp_fmt_str(obj_dir)).value);
  }

  sp_str_t hdr_dir = sp_fs_parent_path(hdr_path);
  if (!sp_str_empty(hdr_dir) && sp_fs_create_dir(hdr_dir) != SP_OK) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to create {.red}", sp_fmt_str(hdr_dir)).value);
  }

  if (sp_fs_create_file_slice(obj_path, (sp_mem_slice_t){ object.storage.data, object.storage.len }) != SP_OK) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to write {.red}", sp_fmt_str(obj_path)).value);
  }

  if (sp_fs_create_file_slice(hdr_path, (sp_mem_slice_t){ header.storage.data, header.storage.len }) != SP_OK) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to write {.red}", sp_fmt_str(hdr_path)).value);
  }

  sp_log("embedded {.yellow} files -> {.gray} ({} bytes), {.gray} ({} bytes)", sp_fmt_uint(sp_da_size(entries)), sp_fmt_str(obj_path), sp_fmt_uint(object.storage.len), sp_fmt_str(hdr_path), sp_fmt_uint(header.storage.len));

  embed_obj_free(&obj);
  return SP_CLI_OK;
}

s32 embed_main(s32 num_args, const c8** args) {
  embed_t embed = sp_zero;

  sp_cli_cmd_t root = {
    .name = "embed",
    .summary = "Embed files into a linkable object",
    .opts = {
      {
        .name = "header",
        .kind = SP_CLI_OPT_STRING,
        .summary = "Path of the C header to write (defaults to <output> with a .h extension)",
        .placeholder = "path",
        .ptr = &embed.header,
      },
      {
        .name = "format",
        .kind = SP_CLI_OPT_STRING,
        .summary = "Object format to write: elf or coff (defaults to elf)",
        .placeholder = "format",
        .ptr = &embed.format,
      },
    },
    .args = {
      {
        .name = "output",
        .summary = "Path of the object file to write",
        .ptr = &embed.output,
      },
      {
        .name = "files",
        .kind = SP_CLI_ARG_REST,
        .summary = "Files or directories to embed (use src=dest to override the embedded path)",
      },
    },
    .handler = embed_run,
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = args,
    .user_data = &embed,
  });
}
SP_MAIN(embed_main)
