#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/coff.h"
#include "sp/sp_elf.h"

typedef struct {
  sp_str_t path;
  sp_str_t symbol;
  u64 size;
} embed_entry_t;

sp_str_t symbol_from_path(sp_str_t path) {
  sp_str_t symbol = sp_str_copy(path);
  symbol = sp_str_replace_c8(symbol, '/', '_');
  symbol = sp_str_replace_c8(symbol, '\\', '_');
  symbol = sp_str_replace_c8(symbol, '.', '_');
  symbol = sp_str_replace_c8(symbol, '-', '_');
  return symbol;
}

#ifdef _WIN32

s32 main(s32 argc, c8** argv) {
  s32 rc = 0;

  if (argc < 4) {
    SP_LOG("usage: embed <src_dir> <output.obj> <output.h> [extra_files...]");
    rc = 1;
    goto cleanup;
  }

  sp_str_t src_dir = sp_str_view(argv[1]);
  sp_str_t out_obj = sp_str_view(argv[2]);
  sp_str_t out_hdr = sp_str_view(argv[3]);
  SP_LOG("scanning {}", SP_FMT_STR(src_dir));

  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(src_dir);
  if (sp_da_empty(files)) {
    SP_LOG("no files found in {}", SP_FMT_STR(src_dir));
    rc = 1;
    goto cleanup;
  }

  sp_coff_t* coff = sp_coff_new();

  sp_coff_section_t* rdata = sp_coff_add_section(coff, sp_str_lit(".rdata"),
    SP_COFF_SCN_CNT_INITIALIZED_DATA |
    SP_COFF_SCN_ALIGN_8BYTES |
    SP_COFF_SCN_MEM_READ);

  sp_da(embed_entry_t) entries = SP_NULLPTR;

  sp_da_for(files, it) {
    sp_fs_entry_t ent = files[it];
    if (ent.kind == SP_FS_KIND_DIR) {
      continue;
    }

    u32 skip = src_dir.len + 1;
    sp_str_t rel_path = sp_str_sub(ent.file_path, skip, ent.file_path.len - skip);
    sp_str_t symbol = symbol_from_path(rel_path);

    sp_str_t contents = sp_io_read_file(ent.file_path);
    u64 size = contents.len;

    u32 off_data = (u32)sp_io_writer_size(&rdata->writer);
    sp_io_write(&rdata->writer, contents.data, size);
    sp_coff_add_symbol(coff, symbol, off_data, 1,SP_COFF_SYM_CLASS_EXTERNAL);

    u32 off_size = (u32)sp_io_writer_size(&rdata->writer);
    sp_io_write(&rdata->writer, &size, sizeof(u64));
    sp_coff_add_symbol(coff, sp_format("{}_size", SP_FMT_STR(symbol)),
      off_size, 1,SP_COFF_SYM_CLASS_EXTERNAL);

    sp_da_push(entries, ((embed_entry_t) {
      .path = rel_path,
      .symbol = symbol,
      .size = size,
    }));
  }

  {
    sp_str_t file_path = sp_str_view(argv[4]);
    sp_str_t symbol = sp_str_lit("include_spn_h");
    sp_str_t path = sp_str_lit("include/spn.h");

    sp_str_t contents = sp_io_read_file(file_path);
    u64 size = contents.len;

    u32 off_data = (u32)sp_io_writer_size(&rdata->writer);
    sp_io_write(&rdata->writer, contents.data, size);
    sp_coff_add_symbol(coff, symbol, off_data, 1,SP_COFF_SYM_CLASS_EXTERNAL);

    u32 off_size = (u32)sp_io_writer_size(&rdata->writer);
    sp_io_write(&rdata->writer, &size, sizeof(u64));
    sp_coff_add_symbol(coff, sp_format("{}_size", SP_FMT_STR(symbol)),
      off_size, 1,SP_COFF_SYM_CLASS_EXTERNAL);

    sp_da_push(entries, ((embed_entry_t) {
      .path = path,
      .symbol = symbol,
      .size = size,
    }));
  }

  sp_err_t err = sp_coff_write_to_file(coff, out_obj);
  if (err != SP_OK) {
    SP_LOG("failed to write {}", SP_FMT_STR(out_obj));
    rc = 1;
    goto cleanup;
  }

  sp_io_writer_t hdr = sp_io_writer_from_file(out_hdr, SP_IO_WRITE_MODE_OVERWRITE);
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_io_write_str(&hdr, sp_format(
      "extern const u8 {} [{}];\n",
      SP_FMT_STR(entry.symbol),
      SP_FMT_U64(entry.size)
    ));
    sp_io_write_str(&hdr, sp_format(
      "extern const u64 {}_size;\n\n",
      SP_FMT_STR(entry.symbol)
    ));
  }
  sp_io_write_str(&hdr, sp_str_lit("typedef struct {\n  const char* path;\n  const void* data;\n  unsigned long long size;\n} spn_embed_entry_t;\n"));
  sp_io_write_str(&hdr, sp_format("static const unsigned int spn_embed_count = {};\n", SP_FMT_U32(sp_da_size(entries))));
  sp_io_write_str(&hdr, sp_str_lit("static const spn_embed_entry_t spn_embed_manifest[] = {\n"));
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_io_write_cstr(&hdr, "  { ");
    sp_io_write_str(&hdr, sp_format("\"{}\", {}, {}", SP_FMT_STR(entry.path), SP_FMT_STR(entry.symbol), SP_FMT_U64(entry.size)));
    sp_io_write_cstr(&hdr, " },\n");
  }
  sp_io_write_str(&hdr, sp_str_lit("};\n"));
  sp_io_writer_close(&hdr);

  SP_LOG("embedded {} files -> {} + {}", SP_FMT_U32(sp_da_size(entries)), SP_FMT_STR(out_obj), SP_FMT_STR(out_hdr));

cleanup:
  return rc;
}

#else // Linux/Unix - ELF

s32 main(s32 argc, c8** argv) {
  s32 rc = 0;

  if (argc < 4) {
    SP_LOG("usage: embed <src_dir> <output.o> <output.h> [extra_files...]");
    rc = 1;
    goto cleanup;
  }

  sp_str_t src_dir = sp_str_view(argv[1]);
  sp_str_t out_obj = sp_str_view(argv[2]);
  sp_str_t out_hdr = sp_str_view(argv[3]);
  SP_LOG("scanning {}", SP_FMT_STR(src_dir));

  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(src_dir);
  if (sp_da_empty(files)) {
    SP_LOG("no files found in {}", SP_FMT_STR(src_dir));
    rc = 1;
    goto cleanup;
  }

  sp_elf_t* elf = sp_elf_new_with_null_section();
  sp_elf_symtab_new(elf);

  u32 rodata_idx = sp_elf_add_section(elf, sp_str_lit(".rodata"), SHT_PROGBITS, 8)->index;
  sp_elf_find_section_by_index(elf, rodata_idx)->flags = SHF_ALLOC | SHF_WRITE;

  sp_da(embed_entry_t) entries = SP_NULLPTR;

  sp_da_for(files, it) {
    sp_fs_entry_t ent = files[it];
    if (ent.kind == SP_FS_KIND_DIR) {
      continue;
    }

    u32 skip = src_dir.len + 1;
    sp_str_t rel_path = sp_str_sub(ent.file_path, skip, ent.file_path.len - skip);
    sp_str_t symbol = symbol_from_path(rel_path);

    sp_io_reader_t io = sp_io_reader_from_file(ent.file_path);
    u64 size = sp_io_reader_size(&io);
    sp_io_reader_seek(&io, 0, SP_IO_SEEK_SET);

    sp_elf_section_t* symtab = sp_elf_find_section_by_name(elf, sp_str_lit(".symtab"));
    sp_elf_section_t* section = sp_elf_find_section_by_index(elf, rodata_idx);

    {
      u64 offset = section->buffer.size;
      u8* ptr = sp_elf_section_reserve_bytes(section, size);
      sp_io_read(&io, ptr, size);

      sp_elf_add_symbol(
        symtab, elf,
        symbol,
        offset, size,
        STB_GLOBAL, STT_OBJECT,
        section->index
      );
    }

    {
      section = sp_elf_find_section_by_index(elf, rodata_idx);
      symtab = sp_elf_find_section_by_name(elf, sp_str_lit(".symtab"));

      u64 offset = section->buffer.size;
      u64* ptr = (u64*)sp_elf_section_reserve_bytes(section, sizeof(u64));
      *ptr = size;

      sp_elf_add_symbol(
        symtab, elf,
        sp_format("{}_size", SP_FMT_STR(symbol)),
        offset, sizeof(u64),
        STB_GLOBAL, STT_OBJECT,
        section->index
      );
    }

    sp_da_push(entries, ((embed_entry_t) {
      .path = rel_path,
      .symbol = symbol,
      .size = size,
    }));

    sp_io_reader_close(&io);
  }

  {
    sp_str_t file_path = sp_str_view(argv[4]);
    sp_str_t symbol = sp_str_lit("include_spn_h");
    sp_str_t path = sp_str_lit("include/spn.h");

    sp_io_reader_t io = sp_io_reader_from_file(file_path);
    u64 size = sp_io_reader_size(&io);
    sp_io_reader_seek(&io, 0, SP_IO_SEEK_SET);

    sp_elf_section_t* symtab = sp_elf_find_section_by_name(elf, sp_str_lit(".symtab"));
    sp_elf_section_t* section = sp_elf_find_section_by_index(elf, rodata_idx);

    {
      u64 offset = section->buffer.size;
      u8* ptr = sp_elf_section_reserve_bytes(section, size);
      sp_io_read(&io, ptr, size);

      sp_elf_add_symbol(
        symtab, elf,
        symbol,
        offset, size,
        STB_GLOBAL, STT_OBJECT,
        section->index
      );
    }

    {
      section = sp_elf_find_section_by_index(elf, rodata_idx);
      symtab = sp_elf_find_section_by_name(elf, sp_str_lit(".symtab"));

      u64 offset = section->buffer.size;
      u64* ptr = (u64*)sp_elf_section_reserve_bytes(section, sizeof(u64));
      *ptr = size;

      sp_elf_add_symbol(
        symtab, elf,
        sp_format("{}_size", SP_FMT_STR(symbol)),
        offset, sizeof(u64),
        STB_GLOBAL, STT_OBJECT,
        section->index
      );
    }

    sp_da_push(entries, ((embed_entry_t) {
      .path = path,
      .symbol = symbol,
      .size = size,
    }));

    sp_io_reader_close(&io);
  }

  sp_err_t err = sp_elf_write_to_file(elf, out_obj);
  if (err != SP_OK) {
    SP_LOG("failed to write {}", SP_FMT_STR(out_obj));
    rc = 1;
    goto cleanup;
  }

  sp_io_writer_t hdr = sp_io_writer_from_file(out_hdr, SP_IO_WRITE_MODE_OVERWRITE);
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_io_write_str(&hdr, sp_format(
      "extern const u8 {} [{}];\n",
      SP_FMT_STR(entry.symbol),
      SP_FMT_U64(entry.size)
    ));
    sp_io_write_str(&hdr, sp_format(
      "extern const u64 {}_size;\n\n",
      SP_FMT_STR(entry.symbol)
    ));
  }
  sp_io_write_str(&hdr, sp_str_lit("typedef struct {\n  const char* path;\n  const void* data;\n  unsigned long long size;\n} spn_embed_entry_t;\n"));
  sp_io_write_str(&hdr, sp_format("static const unsigned int spn_embed_count = {};\n", SP_FMT_U32(sp_da_size(entries))));
  sp_io_write_str(&hdr, sp_str_lit("static const spn_embed_entry_t spn_embed_manifest[] = {\n"));
  sp_da_for(entries, it) {
    embed_entry_t entry = entries[it];
    sp_io_write_cstr(&hdr, "  { ");
    sp_io_write_str(&hdr, sp_format("\"{}\", {}, {}", SP_FMT_STR(entry.path), SP_FMT_STR(entry.symbol), SP_FMT_U64(entry.size)));
    sp_io_write_cstr(&hdr, " },\n");
  }
  sp_io_write_str(&hdr, sp_str_lit("};\n"));
  sp_io_writer_close(&hdr);

  SP_LOG("embedded {} files -> {} + {}", SP_FMT_U32(sp_da_size(entries)), SP_FMT_STR(out_obj), SP_FMT_STR(out_hdr));

cleanup:
  return rc;
}

#endif
