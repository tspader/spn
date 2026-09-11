#include "elf_emit.h"
#include "macro/macro.h"

typedef struct {
  u8 e_ident [16];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  u64 e_entry;
  u64 e_phoff;
  u64 e_shoff;
  u32 e_flags;
  u16 e_ehsize;
  u16 e_phentsize;
  u16 e_phnum;
  u16 e_shentsize;
  u16 e_shnum;
  u16 e_shstrndx;
} elf_ehdr_t;

typedef struct {
  u32 p_type;
  u32 p_flags;
  u64 p_offset;
  u64 p_vaddr;
  u64 p_paddr;
  u64 p_filesz;
  u64 p_memsz;
  u64 p_align;
} elf_phdr_t;

typedef struct {
  u32 sh_name;
  u32 sh_type;
  u64 sh_flags;
  u64 sh_addr;
  u64 sh_offset;
  u64 sh_size;
  u32 sh_link;
  u32 sh_info;
  u64 sh_addralign;
  u64 sh_entsize;
} elf_shdr_t;

typedef struct {
  u32 st_name;
  u8 st_info;
  u8 st_other;
  u16 st_shndx;
  u64 st_value;
  u64 st_size;
} elf_sym_t;

#define ELF_PT_LOAD 1
#define ELF_PT_INTERP 3
#define ELF_SHT_SYMTAB 2
#define ELF_SHT_STRTAB 3
#define ELF_SHN_TEXT 1

typedef struct {
  c8* bytes;
  u64 strtab_off;
  u64 strtab_len;
  u64 symtab_off;
  u32 num_symbols;
} symbols_t;

static u64 names_size(const c8* const* names, u32 count) {
  u64 size = 0;
  sp_for(it, count) {
    size += sp_cstr_len(names[it]) + 1;
  }
  return size;
}

static void put_symbol(symbols_t* out, const c8* name, u16 shndx) {
  u32 len = sp_cstr_len(name);
  sp_mem_copy(out->bytes + out->strtab_off + out->strtab_len, name, len);
  elf_sym_t sym = { .st_name = (u32)out->strtab_len, .st_shndx = shndx };
  out->num_symbols++;
  sp_mem_copy(out->bytes + out->symtab_off + out->num_symbols * sizeof(elf_sym_t), &sym, sizeof(sym));
  out->strtab_len += len + 1;
}

sp_str_t elf_emit(sp_mem_t mem, const elf_spec_t* spec) {
  u32 num_defined = 0;
  sp_carr_detect_len(spec->symbols, num_defined, spec->symbols[num_defined]);
  u32 num_undefined = 0;
  sp_carr_detect_len(spec->undefined, num_undefined, spec->undefined[num_undefined]);
  u32 num_symbols = num_defined + num_undefined;
  u32 num_shdrs = num_symbols ? 3 : 0;

  u32 num_phdrs = (spec->load_first ? 1 : 0) + (spec->interp ? 1 : 0);
  u64 phoff = sizeof(elf_ehdr_t);
  u64 interp_off = phoff + num_phdrs * sizeof(elf_phdr_t);
  sp_str_t interp = spec->interp ? sp_cstr_as_str(spec->interp) : (sp_str_t) sp_zero;
  u64 strtab_off = interp_off + interp.len + 1;
  u64 strtab_size = 1 + names_size(spec->symbols, num_defined) + names_size(spec->undefined, num_undefined);
  u64 symtab_off = strtab_off + strtab_size;
  u64 symtab_size = (num_symbols + 1) * sizeof(elf_sym_t);
  u64 shoff = symtab_off + symtab_size;
  u64 size = shoff + num_shdrs * sizeof(elf_shdr_t);

  c8* bytes = sp_alloc(mem, size);

  elf_ehdr_t* ehdr = (elf_ehdr_t*)bytes;
  ehdr->e_ident[0] = spec->bad_magic ? 0x7e : 0x7f;
  ehdr->e_ident[1] = 'E';
  ehdr->e_ident[2] = 'L';
  ehdr->e_ident[3] = 'F';
  ehdr->e_ident[4] = spec->elf32 ? 1 : 2;
  ehdr->e_entry = spec->entry;
  ehdr->e_phoff = phoff;
  ehdr->e_phentsize = sizeof(elf_phdr_t);
  ehdr->e_phnum = (u16)(spec->truncated ? num_phdrs + 8 : num_phdrs);
  ehdr->e_shoff = num_shdrs ? shoff : 0;
  ehdr->e_shentsize = sizeof(elf_shdr_t);
  ehdr->e_shnum = (u16)num_shdrs;

  elf_phdr_t* phdr = (elf_phdr_t*)(bytes + phoff);
  if (spec->load_first) {
    phdr->p_type = ELF_PT_LOAD;
    phdr++;
  }
  if (spec->interp) {
    phdr->p_type = ELF_PT_INTERP;
    phdr->p_offset = interp_off;
    phdr->p_filesz = interp.len + 1;
    sp_mem_copy(bytes + interp_off, interp.data, interp.len);
  }

  if (num_symbols) {
    symbols_t out = { .bytes = bytes, .strtab_off = strtab_off, .strtab_len = 1, .symtab_off = symtab_off };
    sp_for(it, num_defined) {
      put_symbol(&out, spec->symbols[it], ELF_SHN_TEXT);
    }
    sp_for(it, num_undefined) {
      put_symbol(&out, spec->undefined[it], 0);
    }
    if (spec->bad_name) {
      elf_sym_t bad = { .st_name = (u32)strtab_size, .st_shndx = ELF_SHN_TEXT };
      sp_mem_copy(bytes + symtab_off + sizeof(elf_sym_t), &bad, sizeof(bad));
    }
    elf_shdr_t symtab = {
      .sh_type = ELF_SHT_SYMTAB,
      .sh_offset = symtab_off,
      .sh_size = symtab_size,
      .sh_link = 2,
      .sh_entsize = sizeof(elf_sym_t),
    };
    elf_shdr_t strtab = {
      .sh_type = ELF_SHT_STRTAB,
      .sh_offset = strtab_off,
      .sh_size = strtab_size,
    };
    sp_mem_copy(bytes + shoff + 1 * sizeof(elf_shdr_t), &symtab, sizeof(symtab));
    sp_mem_copy(bytes + shoff + 2 * sizeof(elf_shdr_t), &strtab, sizeof(strtab));
  }

  return sp_str(bytes, (u32)size);
}

sp_io_seeking_reader_t elf_reader(sp_io_reader_t* backing, sp_str_t elf) {
  sp_io_seeking_reader_t reader = sp_zero;
  sp_io_seeking_reader_from_mem(&reader, backing, elf.data, elf.len);
  return reader;
}
