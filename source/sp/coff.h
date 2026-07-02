#ifndef SP_COFF_H
#define SP_COFF_H

#include "sp.h"

#define SP_COFF_MACHINE_AMD64      0x8664
#define SP_COFF_MACHINE_I386       0x014C

#define SP_COFF_SCN_CNT_CODE              0x00000020
#define SP_COFF_SCN_CNT_INITIALIZED_DATA  0x00000040
#define SP_COFF_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define SP_COFF_SCN_ALIGN_1BYTES          0x00100000
#define SP_COFF_SCN_ALIGN_2BYTES          0x00200000
#define SP_COFF_SCN_ALIGN_4BYTES          0x00300000
#define SP_COFF_SCN_ALIGN_8BYTES          0x00400000
#define SP_COFF_SCN_ALIGN_16BYTES         0x00500000
#define SP_COFF_SCN_MEM_EXECUTE           0x20000000
#define SP_COFF_SCN_MEM_READ              0x40000000
#define SP_COFF_SCN_MEM_WRITE             0x80000000

#define SP_COFF_SYM_CLASS_EXTERNAL  2
#define SP_COFF_SYM_CLASS_STATIC    3

#define SP_COFF_SYM_TYPE_NULL       0
#define SP_COFF_SYM_DTYPE_NONE      0

#define SP_COFF_SYM_UNDEFINED       0
#define SP_COFF_SYM_ABSOLUTE       -1

#define SP_COFF_FILE_HEADER_SIZE     20
#define SP_COFF_SECTION_HEADER_SIZE  40
#define SP_COFF_SYMBOL_SIZE          18

typedef struct {
  sp_str_t name;
  u32 flags;
  sp_io_dyn_mem_writer_t writer;
} sp_coff_section_t;

typedef struct {
  sp_str_t name;
  u32 value;
  s16 section_number;  // 1-based
  u16 type;
  u8 storage_class;
  u8 number_of_aux_symbols;
} sp_coff_sym_entry_t;

typedef struct {
  sp_da(sp_coff_section_t) sections;
  sp_da(sp_coff_sym_entry_t) symbols;
  sp_io_dyn_mem_writer_t strtab;
  sp_mem_arena_t* arena;
} sp_coff_t;

sp_coff_t*          sp_coff_new(sp_mem_t mem);
void                sp_coff_free(sp_coff_t* coff);
sp_coff_section_t*  sp_coff_add_section(sp_coff_t* coff, sp_str_t name, u32 flags);
sp_coff_section_t*  sp_coff_find_section(sp_coff_t* coff, sp_str_t name);
void                sp_coff_add_symbol(sp_coff_t* coff, sp_str_t name, u32 value, s16 section_number, u8 storage_class);
sp_err_t            sp_coff_write(sp_coff_t* coff, sp_io_writer_t* out);
sp_err_t            sp_coff_write_to_file(sp_coff_t* coff, sp_str_t path);

#endif // SP_COFF_H


#if defined(SP_IMPLEMENTATION)

static void sp_coff_write_u8(sp_io_writer_t* w, u8 v)   { sp_io_write(w, &v, 1, SP_NULLPTR); }
static void sp_coff_write_u16(sp_io_writer_t* w, u16 v)  { sp_io_write(w, &v, 2, SP_NULLPTR); }
static void sp_coff_write_u32(sp_io_writer_t* w, u32 v)  { sp_io_write(w, &v, 4, SP_NULLPTR); }
static void sp_coff_write_s16(sp_io_writer_t* w, s16 v)  { sp_io_write(w, &v, 2, SP_NULLPTR); }

sp_coff_t* sp_coff_new(sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t alloc = sp_mem_arena_as_allocator(arena);

  sp_coff_t* coff = sp_alloc_type(alloc, sp_coff_t);
  coff->arena = arena;
  sp_da_init(alloc, coff->sections);
  sp_da_init(alloc, coff->symbols);

  // The string table needs a u32 total size at the start of the buffer. We obviously
  // don't know the total size yet, but reserve and zero the four bytes up front.
  sp_io_dyn_mem_writer_init(alloc, &coff->strtab);
  sp_coff_write_u32(&coff->strtab.base, 0);

  return coff;
}

void sp_coff_free(sp_coff_t* coff) {
  if (!coff) return;
  sp_mem_arena_destroy(coff->arena);
}

static u32 sp_coff_add_string(sp_coff_t* coff, sp_str_t str) {
  u64 offset = coff->strtab.storage.len;
  sp_io_write(&coff->strtab.base, str.data, str.len, SP_NULLPTR);
  sp_io_pad(&coff->strtab.base, 1, SP_NULLPTR);
  return (u32)offset;
}

// Write an 8-byte section name. Long names use ASCII "/offset" format.
static void sp_coff_write_section_name(sp_coff_t* coff, sp_io_writer_t* out, sp_str_t name) {
  c8 buf[8] = {0};
  if (name.len <= 8) {
    sp_mem_copy(buf, name.data, name.len);
  } else {
    u32 offset = sp_coff_add_string(coff, name);
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_str_t s = sp_fmt(scratch.mem, "/{}", sp_fmt_uint(offset)).value;
    sp_mem_copy(buf, s.data, sp_min(s.len, 8));
    sp_mem_end_scratch(scratch);
  }
  sp_io_write(out, buf, 8, SP_NULLPTR);
}

// Write an 8-byte symbol name. Long names use binary {0, 0, 0, 0, offset_u32}.
static void sp_coff_write_symbol_name(sp_coff_t* coff, sp_io_writer_t* out, sp_str_t name) {
  if (name.len <= 8) {
    c8 buf[8] = {0};
    sp_mem_copy(buf, name.data, name.len);
    sp_io_write(out, buf, 8, SP_NULLPTR);
  } else {
    sp_coff_write_u32(out, 0);
    sp_coff_write_u32(out, sp_coff_add_string(coff, name));
  }
}

sp_coff_section_t* sp_coff_add_section(sp_coff_t* coff, sp_str_t name, u32 flags) {
  sp_mem_t alloc = sp_mem_arena_as_allocator(coff->arena);
  sp_coff_section_t section = {
    .name = sp_str_copy(alloc, name),
    .flags = flags,
  };
  sp_io_dyn_mem_writer_init(alloc, &section.writer);
  sp_da_push(coff->sections, section);
  return sp_da_back(coff->sections);
}

sp_coff_section_t* sp_coff_find_section(sp_coff_t* coff, sp_str_t name) {
  sp_da_for(coff->sections, i) {
    if (sp_str_equal(coff->sections[i].name, name)) {
      return &coff->sections[i];
    }
  }
  return SP_NULLPTR;
}

void sp_coff_add_symbol(sp_coff_t* coff, sp_str_t name, u32 value, s16 section_number, u8 storage_class) {
  sp_da_push(coff->symbols, ((sp_coff_sym_entry_t) {
    .name = sp_str_copy(sp_mem_arena_as_allocator(coff->arena), name),
    .value = value,
    .section_number = section_number,
    .storage_class = storage_class,
  }));
}

sp_err_t sp_coff_write(sp_coff_t* coff, sp_io_writer_t* out) {
  u16 num_sections = (u16)sp_da_size(coff->sections);
  u32 num_symbols = sp_da_size(coff->symbols);

  u32 headers_end = SP_COFF_FILE_HEADER_SIZE + num_sections * SP_COFF_SECTION_HEADER_SIZE;
  u32 pos = headers_end;

  // Calculate per-section file offsets (respecting declared alignment)
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(u32) offsets = sp_da_new(scratch.mem, u32);
  sp_da_for(coff->sections, i) {
    u32 data_len = (u32)coff->sections[i].writer.storage.len;
    u32 align_bits = (coff->sections[i].flags >> 20) & 0xF;
    u32 alignment = align_bits ? (1u << (align_bits - 1)) : 4;
    pos = (pos + alignment - 1) & ~(alignment - 1);
    sp_da_push(offsets, pos);
    pos += data_len;
  }
  u32 symtab_offset = pos;

  // File header (20 bytes)
  sp_coff_write_u16(out, SP_COFF_MACHINE_AMD64);   // machine
  sp_coff_write_u16(out, num_sections);             // number_of_sections
  sp_coff_write_u32(out, 0);                        // time_date_stamp
  sp_coff_write_u32(out, symtab_offset);            // pointer_to_symbol_table
  sp_coff_write_u32(out, num_symbols);              // number_of_symbols
  sp_coff_write_u16(out, 0);                        // size_of_optional_header
  sp_coff_write_u16(out, 0);                        // characteristics

  // Section headers (40 bytes each)
  sp_da_for(coff->sections, i) {
    sp_coff_section_t* sec = &coff->sections[i];
    u32 sz = (u32)sec->writer.storage.len;

    sp_coff_write_section_name(coff, out, sec->name);
    sp_coff_write_u32(out, 0);                      // virtual_size
    sp_coff_write_u32(out, 0);                      // virtual_address
    sp_coff_write_u32(out, sz);                     // size_of_raw_data
    sp_coff_write_u32(out, sz ? offsets[i] : 0);    // pointer_to_raw_data
    sp_coff_write_u32(out, 0);                      // pointer_to_relocations
    sp_coff_write_u32(out, 0);                      // pointer_to_line_numbers
    sp_coff_write_u16(out, 0);                      // number_of_relocations
    sp_coff_write_u16(out, 0);                      // number_of_line_numbers
    sp_coff_write_u32(out, sec->flags);             // characteristics
  }

  // Section data
  u32 written = headers_end;
  sp_da_for(coff->sections, i) {
    sp_mem_buffer_t buf = coff->sections[i].writer.storage;
    if (buf.len) {
      if (offsets[i] > written) sp_io_pad(out, offsets[i] - written, SP_NULLPTR);
      sp_io_write(out, buf.data, buf.len, SP_NULLPTR);
      written = offsets[i] + (u32)buf.len;
    }
  }
  if (symtab_offset > written) sp_io_pad(out, symtab_offset - written, SP_NULLPTR);

  // Symbol table (18 bytes each)
  sp_da_for(coff->symbols, i) {
    sp_coff_sym_entry_t* sym = &coff->symbols[i];

    sp_coff_write_symbol_name(coff, out, sym->name);

    sp_coff_write_u32(out, sym->value);
    sp_coff_write_s16(out, sym->section_number);
    sp_coff_write_u16(out, sym->type);
    sp_coff_write_u8(out, sym->storage_class);
    sp_coff_write_u8(out, sym->number_of_aux_symbols);
  }

  // String table (patch total size into first 4 bytes, then write)
  sp_mem_buffer_t strtab = coff->strtab.storage;
  u32 strtab_size = (u32)strtab.len;
  sp_mem_copy(strtab.data, &strtab_size, 4);
  sp_io_write(out, strtab.data, strtab.len, SP_NULLPTR);

  sp_mem_end_scratch(scratch);
  return SP_OK;
}

sp_err_t sp_coff_write_to_file(sp_coff_t* coff, sp_str_t path) {
  sp_io_file_writer_t f = sp_zero;
  sp_err_t err = sp_io_file_writer_from_path(&f, path);
  if (err != SP_OK) return err;
  err = sp_coff_write(coff, &f.base);
  sp_io_file_writer_close(&f);
  return err;
}

#endif // SP_IMPLEMENTATION
