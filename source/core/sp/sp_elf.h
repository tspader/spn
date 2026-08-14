#ifndef SP_ELF_H
#define SP_ELF_H

#include "sp.h"

#if !defined(SP_ELF_NO_VENDOR) && !defined(_ELF_H)
typedef u64 Elf64_Addr;
typedef u64 Elf64_Off;
typedef u16 Elf64_Half;
typedef u32 Elf64_Word;
typedef s32 Elf64_Sword;
typedef u64 Elf64_Xword;
typedef s64 Elf64_Sxword;

#define EI_NIDENT 16

typedef struct {
  u8 e_ident[EI_NIDENT];
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off e_phoff;
  Elf64_Off e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  Elf64_Word sh_name;
  Elf64_Word sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr sh_addr;
  Elf64_Off sh_offset;
  Elf64_Xword sh_size;
  Elf64_Word sh_link;
  Elf64_Word sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct {
  Elf64_Word st_name;
  u8 st_info;
  u8 st_other;
  Elf64_Half st_shndx;
  Elf64_Addr st_value;
  Elf64_Xword st_size;
} Elf64_Sym;

typedef struct {
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
  Elf64_Sxword r_addend;
} Elf64_Rela;

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_PAD 8

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATA2LSB 1

#define EV_CURRENT 1

#define ELFOSABI_NONE 0

#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3

#define EM_X86_64 62
#define EM_AARCH64 183

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_NOBITS 8

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MERGE 0x10
#define SHF_STRINGS 0x20
#define SHF_INFO_LINK 0x40

#define SHN_UNDEF 0
#define SHN_LORESERVE 0xff00
#define SHN_ABS 0xfff1
#define SHN_COMMON 0xfff2

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4

#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_PLT32 4
#define R_X86_64_32 10

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)
#define ELF64_ST_INFO(b, t) (((b) << 4) | ((t) & 0xf))

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)
#define ELF64_R_INFO(s, t) (((Elf64_Xword)(s) << 32) | (t))
#endif

#define SP_ELF_NONE 0
#define SP_ELF_MAX_ALIGNMENT (1 << 20)

typedef struct {
  u64 offset;
  u32 symbol;
  u32 type;
  s64 addend;
} sp_elf_reloc_t;

typedef struct {
  sp_str_t name;
  u32 index;
  u32 type;
  u64 flags;
  u64 addr;
  u64 align;
  u32 link;
  u32 info;
  u64 entsize;
  u64 size;
  sp_da(u8) data;
  sp_da(sp_elf_reloc_t) relocs;
} sp_elf_section_t;

typedef struct {
  sp_str_t name;
  u32 section;
  u64 value;
  u64 size;
  u8 bind;
  u8 type;
  u8 other;
} sp_elf_symbol_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  u16 machine;
  u16 filetype;
  u32 flags;
  sp_da(sp_elf_section_t) sections;
  sp_da(sp_elf_symbol_t) symbols;
} sp_elf_t;

SP_API sp_elf_t*          sp_elf_new(sp_mem_t mem);
SP_API void               sp_elf_free(sp_elf_t* elf);
SP_API u32                sp_elf_add_section(sp_elf_t* elf, sp_elf_section_t desc);
SP_API sp_elf_section_t*  sp_elf_section(sp_elf_t* elf, u32 section);
SP_API u32                sp_elf_find_section(sp_elf_t* elf, sp_str_t name);
SP_API u32                sp_elf_num_sections(sp_elf_t* elf);
SP_API u64                sp_elf_append(sp_elf_t* elf, u32 section, const void* data, u64 size);
SP_API u64                sp_elf_append_aligned(sp_elf_t* elf, u32 section, const void* data, u64 size, u64 align);
SP_API u32                sp_elf_add_symbol(sp_elf_t* elf, sp_elf_symbol_t desc);
SP_API sp_elf_symbol_t*   sp_elf_symbol(sp_elf_t* elf, u32 symbol);
SP_API u32                sp_elf_find_symbol(sp_elf_t* elf, sp_str_t name);
SP_API u32                sp_elf_num_symbols(sp_elf_t* elf);
SP_API void               sp_elf_add_reloc(sp_elf_t* elf, u32 section, sp_elf_reloc_t reloc);
SP_API sp_err_t           sp_elf_write(sp_elf_t* elf, sp_io_writer_t* out);
SP_API sp_err_t           sp_elf_write_to_file(sp_elf_t* elf, sp_str_t path);
SP_API sp_err_t           sp_elf_read(sp_mem_t mem, const u8* data, u64 size, sp_elf_t** out);
SP_API sp_err_t           sp_elf_read_from_file(sp_mem_t mem, sp_str_t path, sp_elf_t** out);

#endif


#if defined(SP_IMPLEMENTATION) && !defined(SP_ELF_IMPLEMENTATION)
  #define SP_ELF_IMPLEMENTATION
#endif

#if defined(SP_ELF_IMPLEMENTATION) && !defined(SP_ELF_IMPLEMENTATION_GUARD)
#define SP_ELF_IMPLEMENTATION_GUARD

sp_elf_t* sp_elf_new(sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t am = sp_mem_arena_as_allocator(arena);
  sp_elf_t* elf = sp_mem_allocator_alloc_type(am, sp_elf_t);
  elf->arena = arena;
  elf->mem = am;
  elf->machine = EM_X86_64;
  elf->filetype = ET_REL;
  sp_da_init(am, elf->sections);
  sp_da_init(am, elf->symbols);
  sp_da_push(elf->sections, sp_zero_s(sp_elf_section_t));
  sp_da_push(elf->symbols, sp_zero_s(sp_elf_symbol_t));
  return elf;
}

void sp_elf_free(sp_elf_t* elf) {
  if (!elf) return;
  sp_mem_arena_destroy(elf->arena);
}

SP_PRIVATE u32 sp_elf_push_section(sp_elf_t* elf, sp_elf_section_t desc) {
  desc.name = sp_str_copy(elf->mem, desc.name);
  desc.index = (u32)sp_da_size(elf->sections);
  sp_da_init(elf->mem, desc.data);
  sp_da_init(elf->mem, desc.relocs);
  sp_da_push(elf->sections, desc);
  return desc.index;
}

u32 sp_elf_add_section(sp_elf_t* elf, sp_elf_section_t desc) {
  sp_assert(elf);
  sp_assert(!sp_str_empty(desc.name));
  sp_assert((desc.align & (desc.align - 1)) == 0);
  return sp_elf_push_section(elf, desc);
}

sp_elf_section_t* sp_elf_section(sp_elf_t* elf, u32 section) {
  sp_assert(elf);
  if (section == SP_ELF_NONE) return SP_NULLPTR;
  if (!sp_da_bounds_ok(elf->sections, section)) return SP_NULLPTR;
  return &elf->sections[section];
}

u32 sp_elf_find_section(sp_elf_t* elf, sp_str_t name) {
  sp_assert(elf);
  sp_for_range(it, 1, (u32)sp_da_size(elf->sections)) {
    if (sp_str_equal(elf->sections[it].name, name)) return it;
  }
  return SP_ELF_NONE;
}

u32 sp_elf_num_sections(sp_elf_t* elf) {
  sp_assert(elf);
  return (u32)sp_da_size(elf->sections);
}

u64 sp_elf_append_aligned(sp_elf_t* elf, u32 section, const void* data, u64 size, u64 align) {
  sp_elf_section_t* sec = sp_elf_section(elf, section);
  sp_assert(sec);
  sp_assert(sec->type != SHT_NOBITS);
  if (!align) align = 1;
  sp_assert((align & (align - 1)) == 0);

  u64 head = sp_da_size(sec->data);
  u64 offset = sp_align_offset(head, align);
  u64 total = offset + size;
  sp_da_reserve(sec->data, total);
  sp_da_head(sec->data)->size = total;
  sp_mem_zero(sec->data + head, offset - head);
  if (data) {
    sp_mem_copy(sec->data + offset, data, size);
  } else {
    sp_mem_zero(sec->data + offset, size);
  }
  return offset;
}

u64 sp_elf_append(sp_elf_t* elf, u32 section, const void* data, u64 size) {
  return sp_elf_append_aligned(elf, section, data, size, 1);
}

SP_PRIVATE u32 sp_elf_push_symbol(sp_elf_t* elf, sp_elf_symbol_t desc) {
  desc.name = sp_str_copy(elf->mem, desc.name);
  u32 handle = (u32)sp_da_size(elf->symbols);
  sp_da_push(elf->symbols, desc);
  return handle;
}

u32 sp_elf_add_symbol(sp_elf_t* elf, sp_elf_symbol_t desc) {
  sp_assert(elf);
  if (desc.section < SHN_LORESERVE) {
    sp_assert(desc.section < sp_da_size(elf->sections));
  }
  return sp_elf_push_symbol(elf, desc);
}

sp_elf_symbol_t* sp_elf_symbol(sp_elf_t* elf, u32 symbol) {
  sp_assert(elf);
  if (symbol == SP_ELF_NONE) return SP_NULLPTR;
  if (!sp_da_bounds_ok(elf->symbols, symbol)) return SP_NULLPTR;
  return &elf->symbols[symbol];
}

u32 sp_elf_find_symbol(sp_elf_t* elf, sp_str_t name) {
  sp_assert(elf);
  sp_for_range(it, 1, (u32)sp_da_size(elf->symbols)) {
    if (sp_str_equal(elf->symbols[it].name, name)) return it;
  }
  return SP_ELF_NONE;
}

u32 sp_elf_num_symbols(sp_elf_t* elf) {
  sp_assert(elf);
  return (u32)sp_da_size(elf->symbols);
}

void sp_elf_add_reloc(sp_elf_t* elf, u32 section, sp_elf_reloc_t reloc) {
  sp_elf_section_t* sec = sp_elf_section(elf, section);
  sp_assert(sec);
  sp_assert(reloc.symbol < sp_da_size(elf->symbols));
  sp_da_push(sec->relocs, reloc);
}

SP_PRIVATE u32 sp_elf_write_string(sp_da(u8)* blob, sp_str_t str) {
  if (sp_str_empty(str)) return 0;
  u32 offset = (u32)sp_da_size(*blob);
  sp_for(it, str.len) {
    sp_da_push(*blob, (u8)str.data[it]);
  }
  sp_da_push(*blob, 0);
  return offset;
}

SP_PRIVATE bool sp_elf_read_string(const u8* blob, u64 blob_size, u64 offset, sp_str_t* out) {
  if (offset >= blob_size) return false;
  u64 end = offset;
  while (end < blob_size && blob[end]) end++;
  if (end == blob_size) return false;
  out->data = (const c8*)blob + offset;
  out->len = (u32)(end - offset);
  return true;
}

SP_PRIVATE bool sp_elf_bounds_ok(u64 offset, u64 size, u64 limit) {
  return offset <= limit && size <= limit - offset;
}

SP_PRIVATE sp_err_t sp_elf_write_ex(sp_elf_t* elf, sp_io_writer_t* out, sp_mem_t scratch) {
  u64 num_user = sp_da_size(elf->sections);
  u64 num_syms = sp_da_size(elf->symbols);

  u32 num_relas = 0;
  sp_da_for(elf->sections, it) {
    if (sp_da_size(elf->sections[it].relocs)) num_relas++;
  }
  bool has_symtab = num_syms > 1 || num_relas > 0;

  sp_da_for(elf->symbols, it) {
    sp_elf_symbol_t* sym = &elf->symbols[it];
    if (sym->section < SHN_LORESERVE && sym->section >= num_user) return SP_ERR;
  }

  u64 total = num_user + num_relas + (has_symtab ? 2 : 0) + 1;
  if (total >= SHN_LORESERVE) return SP_ERR;

  u32* sym_map = sp_alloc_n(scratch, u32, num_syms);
  Elf64_Sym* syms = sp_alloc_n(scratch, Elf64_Sym, num_syms);
  sp_da(u8) strtab = sp_da_new(scratch, u8);
  sp_da_push(strtab, 0);

  sym_map[0] = 0;
  syms[0] = sp_zero_s(Elf64_Sym);
  u32 encoded = 1;
  u32 first_global = 1;
  sp_for(pass, 2) {
    sp_for_range(it, 1, num_syms) {
      sp_elf_symbol_t* sym = &elf->symbols[it];
      bool local = sym->bind == STB_LOCAL;
      if (local != (pass == 0)) continue;
      syms[encoded].st_name = sp_elf_write_string(&strtab, sym->name);
      syms[encoded].st_info = (u8)ELF64_ST_INFO(sym->bind, sym->type);
      syms[encoded].st_other = sym->other;
      syms[encoded].st_shndx = (u16)sym->section;
      syms[encoded].st_value = sym->value;
      syms[encoded].st_size = sym->size;
      sym_map[it] = encoded++;
    }
    if (pass == 0) first_global = encoded;
  }

  Elf64_Shdr* headers = sp_alloc_n(scratch, Elf64_Shdr, total);
  const u8** payloads = sp_alloc_n(scratch, const u8*, total);
  sp_mem_zero(headers, total * sizeof(Elf64_Shdr));
  sp_mem_zero(payloads, total * sizeof(const u8*));

  sp_da(u8) shstrtab = sp_da_new(scratch, u8);
  sp_da_push(shstrtab, 0);

  u32 cursor = (u32)num_user;
  u32 shstrtab_index = (u32)total - 1;
  u32 symtab_index = has_symtab ? (u32)num_user + num_relas : 0;
  u32 strtab_index = has_symtab ? symtab_index + 1 : 0;

  sp_da_for(elf->sections, it) {
    sp_elf_section_t* sec = &elf->sections[it];
    Elf64_Shdr* header = &headers[it];
    header->sh_name = sp_elf_write_string(&shstrtab, sec->name);
    header->sh_type = sec->type;
    header->sh_flags = sec->flags;
    header->sh_addr = sec->addr;
    header->sh_size = sec->type == SHT_NOBITS ? sec->size : sp_da_size(sec->data);
    header->sh_link = sec->link;
    header->sh_info = sec->info;
    header->sh_addralign = sec->align;
    header->sh_entsize = sec->entsize;
    payloads[it] = sec->data;

    u64 num_relocs = sp_da_size(sec->relocs);
    if (!num_relocs) continue;

    Elf64_Rela* relas = sp_alloc_n(scratch, Elf64_Rela, num_relocs);
    sp_da_for(sec->relocs, j) {
      sp_elf_reloc_t* rel = &sec->relocs[j];
      if (rel->symbol >= num_syms) return SP_ERR;
      relas[j].r_offset = rel->offset;
      relas[j].r_info = ELF64_R_INFO(sym_map[rel->symbol], rel->type);
      relas[j].r_addend = rel->addend;
    }

    sp_str_t rela_name = sp_fmt(scratch, ".rela{}", sp_fmt_str(sec->name)).value;
    Elf64_Shdr* rela_header = &headers[cursor];
    rela_header->sh_name = sp_elf_write_string(&shstrtab, rela_name);
    rela_header->sh_type = SHT_RELA;
    rela_header->sh_flags = SHF_INFO_LINK;
    rela_header->sh_size = num_relocs * sizeof(Elf64_Rela);
    rela_header->sh_link = symtab_index;
    rela_header->sh_info = (u32)it;
    rela_header->sh_addralign = 8;
    rela_header->sh_entsize = sizeof(Elf64_Rela);
    payloads[cursor] = (const u8*)relas;
    cursor++;
  }

  if (has_symtab) {
    Elf64_Shdr* symtab_header = &headers[symtab_index];
    symtab_header->sh_name = sp_elf_write_string(&shstrtab, sp_str_lit(".symtab"));
    symtab_header->sh_type = SHT_SYMTAB;
    symtab_header->sh_size = encoded * sizeof(Elf64_Sym);
    symtab_header->sh_link = strtab_index;
    symtab_header->sh_info = first_global;
    symtab_header->sh_addralign = 8;
    symtab_header->sh_entsize = sizeof(Elf64_Sym);
    payloads[symtab_index] = (const u8*)syms;

    Elf64_Shdr* strtab_header = &headers[strtab_index];
    strtab_header->sh_name = sp_elf_write_string(&shstrtab, sp_str_lit(".strtab"));
    strtab_header->sh_type = SHT_STRTAB;
    strtab_header->sh_size = sp_da_size(strtab);
    strtab_header->sh_addralign = 1;
    payloads[strtab_index] = strtab;
  }

  Elf64_Shdr* shstrtab_header = &headers[shstrtab_index];
  shstrtab_header->sh_name = sp_elf_write_string(&shstrtab, sp_str_lit(".shstrtab"));
  shstrtab_header->sh_type = SHT_STRTAB;
  shstrtab_header->sh_addralign = 1;
  shstrtab_header->sh_size = sp_da_size(shstrtab);
  payloads[shstrtab_index] = shstrtab;

  u64 shoff = sp_align_offset(sizeof(Elf64_Ehdr), 8);
  u64 offset = shoff + total * sizeof(Elf64_Shdr);
  sp_for(it, (u32)total) {
    Elf64_Shdr* header = &headers[it];
    if (header->sh_type == SHT_NULL || header->sh_type == SHT_NOBITS) continue;
    if (!header->sh_size) continue;
    offset = sp_align_offset(offset, sp_max(header->sh_addralign, 1));
    header->sh_offset = offset;
    offset += header->sh_size;
  }

  Elf64_Ehdr ehdr = sp_zero;
  ehdr.e_ident[EI_MAG0] = ELFMAG0;
  ehdr.e_ident[EI_MAG1] = ELFMAG1;
  ehdr.e_ident[EI_MAG2] = ELFMAG2;
  ehdr.e_ident[EI_MAG3] = ELFMAG3;
  ehdr.e_ident[EI_CLASS] = ELFCLASS64;
  ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
  ehdr.e_ident[EI_VERSION] = EV_CURRENT;
  ehdr.e_ident[EI_OSABI] = ELFOSABI_NONE;
  ehdr.e_type = elf->filetype;
  ehdr.e_machine = elf->machine;
  ehdr.e_version = EV_CURRENT;
  ehdr.e_shoff = shoff;
  ehdr.e_flags = elf->flags;
  ehdr.e_ehsize = sizeof(Elf64_Ehdr);
  ehdr.e_shentsize = sizeof(Elf64_Shdr);
  ehdr.e_shnum = (u16)total;
  ehdr.e_shstrndx = (u16)shstrtab_index;

  u64 written = 0;
  u64 n = 0;
  sp_try(sp_io_write(out, &ehdr, sizeof(Elf64_Ehdr), &n));
  written += n;
  sp_try(sp_io_pad(out, shoff - written, &n));
  written += n;

  sp_for(it, (u32)total) {
    sp_try(sp_io_write(out, &headers[it], sizeof(Elf64_Shdr), &n));
    written += n;
  }

  sp_for(it, (u32)total) {
    Elf64_Shdr* header = &headers[it];
    if (header->sh_type == SHT_NULL || header->sh_type == SHT_NOBITS) continue;
    if (!header->sh_size) continue;
    sp_try(sp_io_pad(out, header->sh_offset - written, &n));
    written += n;
    sp_try(sp_io_write(out, payloads[it], header->sh_size, &n));
    written += n;
  }

  return SP_OK;
}

sp_err_t sp_elf_write(sp_elf_t* elf, sp_io_writer_t* out) {
  if(!elf) return SP_ERR_LAZY;
  if(!out) return SP_ERR_LAZY;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(elf->mem);
  sp_err_t err = sp_elf_write_ex(elf, out, scratch.mem);
  sp_mem_end_scratch(scratch);
  return err;
}

sp_err_t sp_elf_write_to_file(sp_elf_t* elf, sp_str_t path) {
  sp_io_file_writer_t f = sp_zero;
  sp_try(sp_io_file_writer_from_path(&f, path));
  sp_err_t err = sp_elf_write(elf, &f.base);
  sp_io_file_writer_close(&f);
  return err;
}

SP_PRIVATE sp_err_t sp_elf_read_ex(sp_elf_t* elf, const u8* data, u64 size, sp_mem_t scratch) {
  if (size < sizeof(Elf64_Ehdr)) return SP_ERR;

  Elf64_Ehdr ehdr;
  sp_mem_copy(&ehdr, data, sizeof(Elf64_Ehdr));

  if (ehdr.e_ident[EI_MAG0] != ELFMAG0) return SP_ERR;
  if (ehdr.e_ident[EI_MAG1] != ELFMAG1) return SP_ERR;
  if (ehdr.e_ident[EI_MAG2] != ELFMAG2) return SP_ERR;
  if (ehdr.e_ident[EI_MAG3] != ELFMAG3) return SP_ERR;
  if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) return SP_ERR;
  if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) return SP_ERR;
  if (ehdr.e_ident[EI_VERSION] != EV_CURRENT) return SP_ERR;

  elf->machine = ehdr.e_machine;
  elf->filetype = ehdr.e_type;
  elf->flags = ehdr.e_flags;

  u64 shnum = ehdr.e_shnum;
  if (!shnum) {
    if (ehdr.e_shoff) return SP_ERR;
    return SP_OK;
  }

  if (ehdr.e_shentsize != sizeof(Elf64_Shdr)) return SP_ERR;
  if (!sp_elf_bounds_ok(ehdr.e_shoff, shnum * sizeof(Elf64_Shdr), size)) return SP_ERR;

  Elf64_Shdr* shdrs = sp_alloc_n(scratch, Elf64_Shdr, shnum);
  sp_mem_copy(shdrs, data + ehdr.e_shoff, shnum * sizeof(Elf64_Shdr));
  if (shdrs[0].sh_type != SHT_NULL) return SP_ERR;

  const u8* shstr = SP_NULLPTR;
  u64 shstr_size = 0;
  if (ehdr.e_shstrndx != SHN_UNDEF) {
    if (ehdr.e_shstrndx >= shnum) return SP_ERR;
    Elf64_Shdr* header = &shdrs[ehdr.e_shstrndx];
    if (!sp_elf_bounds_ok(header->sh_offset, header->sh_size, size)) return SP_ERR;
    shstr = data + header->sh_offset;
    shstr_size = header->sh_size;
  }

  u32 symtab_idx = 0;
  u32 strtab_idx = 0;
  sp_for_range(it, 1, (u32)shnum) {
    if (shdrs[it].sh_type == SHT_SYMTAB) {
      symtab_idx = it;
      break;
    }
  }
  if (symtab_idx) {
    strtab_idx = shdrs[symtab_idx].sh_link;
    if (strtab_idx >= shnum) return SP_ERR;
  }

  bool* dropped = sp_alloc_n(scratch, bool, shnum);
  sp_mem_zero(dropped, shnum * sizeof(bool));
  if (ehdr.e_shstrndx != SHN_UNDEF) dropped[ehdr.e_shstrndx] = true;
  if (symtab_idx) dropped[symtab_idx] = true;
  if (strtab_idx) dropped[strtab_idx] = true;

  sp_for_range(it, 1, (u32)shnum) {
    Elf64_Shdr* header = &shdrs[it];
    if (header->sh_type != SHT_RELA) continue;
    if (!symtab_idx || header->sh_link != symtab_idx) continue;
    if (header->sh_info == 0 || header->sh_info >= shnum) continue;
    if (dropped[header->sh_info]) continue;
    dropped[it] = true;
  }

  u32* map = sp_alloc_n(scratch, u32, shnum);
  sp_mem_zero(map, shnum * sizeof(u32));
  sp_for_range(it, 1, (u32)shnum) {
    if (dropped[it]) continue;
    Elf64_Shdr* header = &shdrs[it];

    if (header->sh_addralign & (header->sh_addralign - 1)) return SP_ERR;
    if (header->sh_addralign > SP_ELF_MAX_ALIGNMENT) return SP_ERR;

    sp_str_t name = sp_zero;
    if (header->sh_name) {
      if (!sp_elf_read_string(shstr, shstr_size, header->sh_name, &name)) return SP_ERR;
    }

    sp_elf_section_t desc = sp_zero;
    desc.name = name;
    desc.type = header->sh_type;
    desc.flags = header->sh_flags;
    desc.addr = header->sh_addr;
    desc.align = header->sh_addralign;
    desc.link = header->sh_link;
    desc.info = header->sh_info;
    desc.entsize = header->sh_entsize;
    desc.size = header->sh_type == SHT_NOBITS ? header->sh_size : 0;
    map[it] = sp_elf_push_section(elf, desc);

    if (header->sh_type != SHT_NOBITS && header->sh_size) {
      if (!sp_elf_bounds_ok(header->sh_offset, header->sh_size, size)) return SP_ERR;
      sp_elf_append(elf, map[it], data + header->sh_offset, header->sh_size);
    }
  }

  sp_for_range(it, 1, (u32)shnum) {
    if (dropped[it]) continue;
    sp_elf_section_t* sec = &elf->sections[map[it]];
    if (sec->link < shnum) sec->link = map[sec->link];
    if ((sec->flags & SHF_INFO_LINK) && sec->info < shnum) sec->info = map[sec->info];
  }

  if (symtab_idx) {
    Elf64_Shdr* header = &shdrs[symtab_idx];
    if (header->sh_entsize && header->sh_entsize != sizeof(Elf64_Sym)) return SP_ERR;
    if (!sp_elf_bounds_ok(header->sh_offset, header->sh_size, size)) return SP_ERR;

    const u8* strblob = SP_NULLPTR;
    u64 strblob_size = 0;
    if (strtab_idx) {
      Elf64_Shdr* strtab_header = &shdrs[strtab_idx];
      if (!sp_elf_bounds_ok(strtab_header->sh_offset, strtab_header->sh_size, size)) return SP_ERR;
      strblob = data + strtab_header->sh_offset;
      strblob_size = strtab_header->sh_size;
    }

    u64 num_file_syms = header->sh_size / sizeof(Elf64_Sym);
    Elf64_Sym* file_syms = sp_alloc_n(scratch, Elf64_Sym, num_file_syms);
    sp_mem_copy(file_syms, data + header->sh_offset, num_file_syms * sizeof(Elf64_Sym));

    sp_for_range(it, 1, (u32)num_file_syms) {
      Elf64_Sym* sym = &file_syms[it];

      sp_str_t name = sp_zero;
      if (sym->st_name) {
        if (!sp_elf_read_string(strblob, strblob_size, sym->st_name, &name)) return SP_ERR;
      }

      u32 section = sym->st_shndx;
      if (section < shnum) section = map[section];

      sp_elf_symbol_t desc = sp_zero;
      desc.name = name;
      desc.section = section;
      desc.value = sym->st_value;
      desc.size = sym->st_size;
      desc.bind = (u8)ELF64_ST_BIND(sym->st_info);
      desc.type = (u8)ELF64_ST_TYPE(sym->st_info);
      desc.other = sym->st_other;
      sp_elf_push_symbol(elf, desc);
    }
  }

  sp_for_range(it, 1, (u32)shnum) {
    Elf64_Shdr* header = &shdrs[it];
    if (!dropped[it] || header->sh_type != SHT_RELA) continue;
    if (header->sh_entsize && header->sh_entsize != sizeof(Elf64_Rela)) return SP_ERR;
    if (!sp_elf_bounds_ok(header->sh_offset, header->sh_size, size)) return SP_ERR;

    u64 count = header->sh_size / sizeof(Elf64_Rela);
    Elf64_Rela* relas = sp_alloc_n(scratch, Elf64_Rela, count);
    sp_mem_copy(relas, data + header->sh_offset, count * sizeof(Elf64_Rela));

    u32 target = map[header->sh_info];
    if (target == SP_ELF_NONE) return SP_ERR;
    sp_for(j, (u32)count) {
      u64 symbol = ELF64_R_SYM(relas[j].r_info);
      if (symbol >= sp_da_size(elf->symbols)) return SP_ERR;

      sp_elf_reloc_t reloc = sp_zero;
      reloc.offset = relas[j].r_offset;
      reloc.symbol = (u32)symbol;
      reloc.type = (u32)ELF64_R_TYPE(relas[j].r_info);
      reloc.addend = relas[j].r_addend;
      sp_da_push(elf->sections[target].relocs, reloc);
    }
  }

  return SP_OK;
}

sp_err_t sp_elf_read(sp_mem_t mem, const u8* data, u64 size, sp_elf_t** out) {
  sp_require_as(data, SP_ERR_LAZY);
  sp_require_as(out, SP_ERR_LAZY);
  *out = SP_NULLPTR;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_elf_t* elf = sp_elf_new(mem);
  sp_err_t err = sp_elf_read_ex(elf, data, size, scratch.mem);
  sp_mem_end_scratch(scratch);

  if (err != SP_OK) {
    sp_elf_free(elf);
    return err;
  }

  *out = elf;
  return SP_OK;
}

sp_err_t sp_elf_read_from_file(sp_mem_t mem, sp_str_t path, sp_elf_t** out) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_str_t bytes = sp_zero;
  sp_err_t err = sp_io_read_file(scratch.mem, path, &bytes);
  if (err == SP_OK) {
    err = sp_elf_read(mem, (const u8*)bytes.data, bytes.len, out);
  }
  sp_mem_end_scratch(scratch);
  return err;
}

#endif
