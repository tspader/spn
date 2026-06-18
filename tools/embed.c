#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_elf.h"

typedef struct {
  sp_str_t path;
  sp_str_t symbol;
  u64 size;
} embed_entry_t;

static sp_str_t symbol_from_path(sp_mem_t mem, sp_str_t path) {
  sp_str_t symbol = sp_str_replace_c8(mem, path, '/', '_');
  symbol = sp_str_replace_c8(mem, symbol, '\\', '_');
  symbol = sp_str_replace_c8(mem, symbol, '.', '_');
  symbol = sp_str_replace_c8(mem, symbol, '-', '_');
  return symbol;
}

static void embed_one(
  sp_mem_t mem,
  sp_elf_t* elf,
  u32 rodata,
  sp_da(embed_entry_t)* entries,
  sp_str_t rel_path,
  sp_str_t contents
) {
  sp_str_t symbol = symbol_from_path(mem, rel_path);
  u64 size = contents.len;

  u64 off_data = sp_elf_append_aligned(elf, rodata, contents.data, size, 8);
  sp_elf_add_symbol(elf, (sp_elf_symbol_t){
    .name = symbol,
    .section = rodata,
    .value = off_data,
    .size = size,
    .bind = STB_GLOBAL,
    .type = STT_OBJECT,
  });

  u64 off_size = sp_elf_append_aligned(elf, rodata, &size, sizeof(u64), 8);
  sp_elf_add_symbol(elf, (sp_elf_symbol_t){
    .name = sp_fmt(mem, "{}_size", sp_fmt_str(symbol)).value,
    .section = rodata,
    .value = off_size,
    .size = sizeof(u64),
    .bind = STB_GLOBAL,
    .type = STT_OBJECT,
  });

  sp_da_push(*entries, ((embed_entry_t) {
    .path = rel_path,
    .symbol = symbol,
    .size = size,
  }));
}

s32 main(s32 argc, c8** argv) {
  if (argc < 4) {
    sp_fmt_std_err("usage: embed <src_dir> <output.o> <output.h> [extra_files...]\n");
    return 1;
  }

  sp_mem_t mem = sp_mem_os_new();
  sp_str_t src_dir = sp_str_view(argv[1]);
  sp_str_t out_obj = sp_str_view(argv[2]);
  sp_str_t out_hdr = sp_str_view(argv[3]);

  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(mem, src_dir);
  if (sp_da_empty(files)) {
    sp_fmt_std_err("no files found in {}\n", sp_fmt_str(src_dir));
    return 1;
  }

  sp_elf_t* elf = sp_elf_new(mem);
  u32 rodata = sp_elf_add_section(elf, (sp_elf_section_t){
    .name = sp_str_lit(".rodata"),
    .type = SHT_PROGBITS,
    .flags = SHF_ALLOC,
    .align = 8,
  });

  sp_da(embed_entry_t) entries = SP_NULLPTR;
  sp_da_init(mem, entries);

  sp_da_for(files, it) {
    sp_fs_entry_t ent = files[it];
    if (ent.kind == SP_FS_KIND_DIR) {
      continue;
    }

    u32 skip = src_dir.len + 1;
    sp_str_t rel_path = sp_str_sub(ent.path, skip, ent.path.len - skip);

    sp_str_t contents = sp_zero;
    if (sp_io_read_file(mem, ent.path, &contents) != SP_OK) {
      sp_fmt_std_err("failed to read {}\n", sp_fmt_str(ent.path));
      return 1;
    }

    embed_one(mem, elf, rodata, &entries, rel_path, contents);
  }

  {
    sp_str_t file_path = sp_str_view(argv[4]);
    sp_str_t contents = sp_zero;
    if (sp_io_read_file(mem, file_path, &contents) != SP_OK) {
      sp_fmt_std_err("failed to read {}\n", sp_fmt_str(file_path));
      return 1;
    }
    embed_one(mem, elf, rodata, &entries, sp_str_lit("include/spn.h"), contents);
  }

  sp_io_dyn_mem_writer_t object;
  sp_io_dyn_mem_writer_init(mem, &object);
  if (sp_elf_write(elf, &object.base) != SP_OK) {
    sp_fmt_std_err("failed to encode {}\n", sp_fmt_str(out_obj));
    return 1;
  }
  sp_str_t obj_bytes = sp_io_dyn_mem_writer_as_str(&object);
  if (sp_fs_create_file_slice(out_obj, sp_mem_slice((u8*)obj_bytes.data, obj_bytes.len)) != SP_OK) {
    sp_fmt_std_err("failed to write {}\n", sp_fmt_str(out_obj));
    return 1;
  }

  sp_io_dyn_mem_writer_t hdr;
  sp_io_dyn_mem_writer_init(mem, &hdr);
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_fmt_io(&hdr.base, "extern const u8 {} [{}];\n", sp_fmt_str(entry.symbol), sp_fmt_uint(entry.size));
    sp_fmt_io(&hdr.base, "extern const u64 {}_size;\n\n", sp_fmt_str(entry.symbol));
  }
  sp_fmt_io(&hdr.base, "typedef struct {{\n  const char* path;\n  const void* data;\n  unsigned long long size;\n}} spn_embed_entry_t;\n");
  sp_fmt_io(&hdr.base, "static const unsigned int spn_embed_count = {};\n", sp_fmt_uint(sp_da_size(entries)));
  sp_fmt_io(&hdr.base, "static const spn_embed_entry_t spn_embed_manifest[] = {{\n");
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_fmt_io(&hdr.base, "  {{ \"{}\", {}, {} }},\n", sp_fmt_str(entry.path), sp_fmt_str(entry.symbol), sp_fmt_uint(entry.size));
  }
  sp_fmt_io(&hdr.base, "}};\n");

  sp_str_t hdr_bytes = sp_io_dyn_mem_writer_as_str(&hdr);
  if (sp_fs_create_file_slice(out_hdr, sp_mem_slice((u8*)hdr_bytes.data, hdr_bytes.len)) != SP_OK) {
    sp_fmt_std_err("failed to write {}\n", sp_fmt_str(out_hdr));
    return 1;
  }

  sp_fmt_std_out("embedded {} files -> {} + {}\n", sp_fmt_uint(sp_da_size(entries)), sp_fmt_str(out_obj), sp_fmt_str(out_hdr));

  sp_elf_free(elf);
  return 0;
}
