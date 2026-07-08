#ifndef SP_MACHO_H
#define SP_MACHO_H

#include "sp.h"

#define SP_MACHO_MAGIC_64                    0xFEEDFACFu
#define SP_MACHO_CPU_X86_64                  0x01000007u
#define SP_MACHO_CPU_ARM64                   0x0100000Cu
#define SP_MACHO_SUBTYPE_X86_64              0x00000003u
#define SP_MACHO_SUBTYPE_ARM64               0x00000000u
#define SP_MACHO_MH_OBJECT                   1
#define SP_MACHO_MH_SUBSECTIONS_VIA_SYMBOLS  0x2000u

#define SP_MACHO_LC_SYMTAB                   0x02u
#define SP_MACHO_LC_SEGMENT_64               0x19u
#define SP_MACHO_LC_BUILD_VERSION            0x32u
#define SP_MACHO_PLATFORM_MACOS              1

#define SP_MACHO_N_EXT                       0x01
#define SP_MACHO_N_SECT                      0x0E

#define SP_MACHO_HEADER_SIZE                 32
#define SP_MACHO_SEGMENT_64_SIZE             72
#define SP_MACHO_SECTION_64_SIZE             80
#define SP_MACHO_BUILD_VERSION_SIZE          24
#define SP_MACHO_SYMTAB_SIZE                 24
#define SP_MACHO_NLIST_64_SIZE               16

typedef struct {
  sp_str_t segname;
  sp_str_t sectname;
  u32 align;  // log2
  sp_io_dyn_mem_writer_t writer;
} sp_macho_section_t;

typedef struct {
  sp_str_t name;
  u8 section;  // 1-based
  u64 value;   // offset within section
} sp_macho_sym_entry_t;

typedef struct {
  u32 cputype;
  u32 cpusubtype;
  sp_da(sp_macho_section_t) sections;
  sp_da(sp_macho_sym_entry_t) symbols;
  sp_mem_arena_t* arena;
} sp_macho_t;

sp_macho_t*          sp_macho_new(sp_mem_t mem, u32 cputype, u32 cpusubtype);
void                 sp_macho_free(sp_macho_t* macho);
sp_macho_section_t*  sp_macho_add_section(sp_macho_t* macho, sp_str_t segname, sp_str_t sectname, u32 align);
void                 sp_macho_add_symbol(sp_macho_t* macho, sp_str_t name, u8 section, u64 value);
sp_err_t             sp_macho_write(sp_macho_t* macho, sp_io_writer_t* out);

#endif // SP_MACHO_H


#if defined(SP_IMPLEMENTATION)

static void sp_macho_write_u32(sp_io_writer_t* w, u32 v) { sp_io_write(w, &v, 4, SP_NULLPTR); }
static void sp_macho_write_u64(sp_io_writer_t* w, u64 v) { sp_io_write(w, &v, 8, SP_NULLPTR); }

// segname/sectname are fixed 16-byte fields, zero padded, not terminated
static void sp_macho_write_name16(sp_io_writer_t* w, sp_str_t name) {
  c8 buf[16] = {0};
  sp_mem_copy(buf, name.data, sp_min(name.len, 16));
  sp_io_write(w, buf, 16, SP_NULLPTR);
}

sp_macho_t* sp_macho_new(sp_mem_t mem, u32 cputype, u32 cpusubtype) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t alloc = sp_mem_arena_as_allocator(arena);

  sp_macho_t* macho = sp_alloc_type(alloc, sp_macho_t);
  macho->arena = arena;
  macho->cputype = cputype;
  macho->cpusubtype = cpusubtype;
  sp_da_init(alloc, macho->sections);
  sp_da_init(alloc, macho->symbols);
  return macho;
}

void sp_macho_free(sp_macho_t* macho) {
  if (!macho) return;
  sp_mem_arena_destroy(macho->arena);
}

sp_macho_section_t* sp_macho_add_section(sp_macho_t* macho, sp_str_t segname, sp_str_t sectname, u32 align) {
  sp_mem_t alloc = sp_mem_arena_as_allocator(macho->arena);
  sp_macho_section_t section = {
    .segname = sp_str_copy(alloc, segname),
    .sectname = sp_str_copy(alloc, sectname),
    .align = align,
  };
  sp_io_dyn_mem_writer_init(alloc, &section.writer);
  sp_da_push(macho->sections, section);
  return sp_da_back(macho->sections);
}

void sp_macho_add_symbol(sp_macho_t* macho, sp_str_t name, u8 section, u64 value) {
  sp_da_push(macho->symbols, ((sp_macho_sym_entry_t) {
    .name = sp_str_copy(sp_mem_arena_as_allocator(macho->arena), name),
    .section = section,
    .value = value,
  }));
}

sp_err_t sp_macho_write(sp_macho_t* macho, sp_io_writer_t* out) {
  u32 nsects = sp_da_size(macho->sections);
  u32 nsyms = sp_da_size(macho->symbols);

  u32 sizeofcmds = SP_MACHO_SEGMENT_64_SIZE + nsects * SP_MACHO_SECTION_64_SIZE
    + SP_MACHO_BUILD_VERSION_SIZE + SP_MACHO_SYMTAB_SIZE;
  u32 data_start = SP_MACHO_HEADER_SIZE + sizeofcmds;

  // Section vm addresses (cumulative, respecting alignment); file offsets
  // mirror them at data_start + addr, so padding is identical in both views.
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(u64) addrs = sp_da_new(scratch.mem, u64);
  u64 addr = 0;
  sp_da_for(macho->sections, i) {
    u64 alignment = 1ull << macho->sections[i].align;
    addr = (addr + alignment - 1) & ~(alignment - 1);
    sp_da_push(addrs, addr);
    addr += macho->sections[i].writer.storage.len;
  }
  u64 vmsize = addr;

  u32 symoff = data_start + (u32)vmsize;
  symoff = (symoff + 7) & ~7u;
  u32 stroff = symoff + nsyms * SP_MACHO_NLIST_64_SIZE;

  // String table: index 0 is reserved for the empty name
  sp_io_dyn_mem_writer_t strtab;
  sp_io_dyn_mem_writer_init(scratch.mem, &strtab);
  sp_io_pad(&strtab.base, 1, SP_NULLPTR);
  sp_da(u32) name_offsets = sp_da_new(scratch.mem, u32);
  sp_da_for(macho->symbols, i) {
    sp_da_push(name_offsets, (u32)strtab.storage.len);
    sp_io_write(&strtab.base, macho->symbols[i].name.data, macho->symbols[i].name.len, SP_NULLPTR);
    sp_io_pad(&strtab.base, 1, SP_NULLPTR);
  }

  // mach_header_64 (32 bytes)
  sp_macho_write_u32(out, SP_MACHO_MAGIC_64);
  sp_macho_write_u32(out, macho->cputype);
  sp_macho_write_u32(out, macho->cpusubtype);
  sp_macho_write_u32(out, SP_MACHO_MH_OBJECT);
  sp_macho_write_u32(out, 3);  // ncmds
  sp_macho_write_u32(out, sizeofcmds);
  sp_macho_write_u32(out, SP_MACHO_MH_SUBSECTIONS_VIA_SYMBOLS);
  sp_macho_write_u32(out, 0);  // reserved

  // LC_SEGMENT_64; MH_OBJECT uses a single unnamed segment holding all sections
  sp_macho_write_u32(out, SP_MACHO_LC_SEGMENT_64);
  sp_macho_write_u32(out, SP_MACHO_SEGMENT_64_SIZE + nsects * SP_MACHO_SECTION_64_SIZE);
  sp_macho_write_name16(out, sp_str_lit(""));
  sp_macho_write_u64(out, 0);           // vmaddr
  sp_macho_write_u64(out, vmsize);
  sp_macho_write_u64(out, data_start);  // fileoff
  sp_macho_write_u64(out, vmsize);      // filesize
  sp_macho_write_u32(out, 7);           // maxprot rwx
  sp_macho_write_u32(out, 7);           // initprot rwx
  sp_macho_write_u32(out, nsects);
  sp_macho_write_u32(out, 0);           // flags

  // section_64 (80 bytes each)
  sp_da_for(macho->sections, i) {
    sp_macho_section_t* sec = &macho->sections[i];
    sp_macho_write_name16(out, sec->sectname);
    sp_macho_write_name16(out, sec->segname);
    sp_macho_write_u64(out, addrs[i]);
    sp_macho_write_u64(out, sec->writer.storage.len);
    sp_macho_write_u32(out, data_start + (u32)addrs[i]);  // offset
    sp_macho_write_u32(out, sec->align);
    sp_macho_write_u32(out, 0);  // reloff
    sp_macho_write_u32(out, 0);  // nreloc
    sp_macho_write_u32(out, 0);  // flags (S_REGULAR)
    sp_macho_write_u32(out, 0);  // reserved1
    sp_macho_write_u32(out, 0);  // reserved2
    sp_macho_write_u32(out, 0);  // reserved3
  }

  // LC_BUILD_VERSION (24 bytes); ld warns on objects with no platform
  sp_macho_write_u32(out, SP_MACHO_LC_BUILD_VERSION);
  sp_macho_write_u32(out, SP_MACHO_BUILD_VERSION_SIZE);
  sp_macho_write_u32(out, SP_MACHO_PLATFORM_MACOS);
  sp_macho_write_u32(out, 11 << 16);  // minos 11.0.0
  sp_macho_write_u32(out, 11 << 16);  // sdk 11.0.0
  sp_macho_write_u32(out, 0);         // ntools

  // LC_SYMTAB (24 bytes)
  sp_macho_write_u32(out, SP_MACHO_LC_SYMTAB);
  sp_macho_write_u32(out, SP_MACHO_SYMTAB_SIZE);
  sp_macho_write_u32(out, symoff);
  sp_macho_write_u32(out, nsyms);
  sp_macho_write_u32(out, stroff);
  sp_macho_write_u32(out, (u32)strtab.storage.len);

  // Section data
  u64 written = data_start;
  sp_da_for(macho->sections, i) {
    sp_mem_buffer_t buf = macho->sections[i].writer.storage;
    u64 offset = data_start + addrs[i];
    if (offset > written) sp_io_pad(out, offset - written, SP_NULLPTR);
    sp_io_write(out, buf.data, buf.len, SP_NULLPTR);
    written = offset + buf.len;
  }
  if (symoff > written) sp_io_pad(out, symoff - written, SP_NULLPTR);

  // nlist_64 (16 bytes each); n_value is the symbol's address, not its
  // section-relative offset
  sp_da_for(macho->symbols, i) {
    sp_macho_sym_entry_t* sym = &macho->symbols[i];
    sp_macho_write_u32(out, name_offsets[i]);
    u8 n_type = SP_MACHO_N_SECT | SP_MACHO_N_EXT;
    sp_io_write(out, &n_type, 1, SP_NULLPTR);
    sp_io_write(out, &sym->section, 1, SP_NULLPTR);
    u16 n_desc = 0;
    sp_io_write(out, &n_desc, 2, SP_NULLPTR);
    sp_macho_write_u64(out, addrs[sym->section - 1] + sym->value);
  }

  sp_io_write(out, strtab.storage.data, strtab.storage.len, SP_NULLPTR);

  sp_mem_end_scratch(scratch);
  return SP_OK;
}

#endif // SP_IMPLEMENTATION
