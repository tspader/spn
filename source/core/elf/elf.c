#include "elf/elf.h"

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

#define SPN_ELF_PT_INTERP 3
#define SPN_ELF_CLASS_64 2
#define SPN_ELF_SHT_SYMTAB 2
#define SPN_ELF_SHN_UNDEF 0

static spn_err_t read_at(sp_io_seeking_reader_t* elf, u64 offset, void* data, u64 size) {
  s64 position = 0;
  u64 bytes = 0;
  if (sp_io_seeking_reader_seek(elf, (s64)offset, SP_IO_SEEK_SET, &position)) {
    return SPN_ERROR;
  }
  if (sp_io_read_all(elf->reader, data, size, &bytes) || bytes != size) {
    return SPN_ERROR;
  }
  return SPN_OK;
}

static spn_err_t read_header(sp_io_seeking_reader_t* elf, elf_ehdr_t* ehdr) {
  spn_try(read_at(elf, 0, ehdr, sizeof(*ehdr)));
  if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' || ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    return SPN_ERROR;
  }
  if (ehdr->e_ident[4] != SPN_ELF_CLASS_64) {
    return SPN_ERROR;
  }
  return SPN_OK;
}

static spn_err_t read_section(sp_io_seeking_reader_t* elf, const elf_ehdr_t* ehdr, u32 index, elf_shdr_t* shdr) {
  return read_at(elf, ehdr->e_shoff + index * ehdr->e_shentsize, shdr, sizeof(*shdr));
}

spn_err_t spn_elf_entry(sp_io_seeking_reader_t* elf, u64* entry) {
  elf_ehdr_t ehdr = sp_zero;
  spn_try(read_header(elf, &ehdr));
  *entry = ehdr.e_entry;
  return SPN_OK;
}

spn_err_t spn_elf_interp(sp_mem_t mem, sp_io_seeking_reader_t* elf, sp_str_t* interp) {
  *interp = sp_str_lit("");

  elf_ehdr_t ehdr = sp_zero;
  spn_try(read_header(elf, &ehdr));

  u64 interp_offset = 0;
  u64 interp_size = 0;
  sp_for(it, ehdr.e_phnum) {
    elf_phdr_t phdr = sp_zero;
    spn_try(read_at(elf, ehdr.e_phoff + it * ehdr.e_phentsize, &phdr, sizeof(phdr)));
    if (phdr.p_type == SPN_ELF_PT_INTERP && !interp_size) {
      interp_offset = phdr.p_offset;
      interp_size = phdr.p_filesz;
    }
  }

  if (!interp_size) {
    return SPN_OK;
  }

  c8* data = sp_alloc(mem, interp_size);
  spn_try(read_at(elf, interp_offset, data, interp_size));

  u32 len = 0;
  while (len < interp_size && data[len]) {
    len++;
  }
  *interp = sp_str(data, len);
  return SPN_OK;
}

static spn_err_t find_symtab(sp_io_seeking_reader_t* elf, const elf_ehdr_t* ehdr, elf_shdr_t* symtab) {
  sp_for(it, ehdr->e_shnum) {
    spn_try(read_section(elf, ehdr, it, symtab));
    if (symtab->sh_type == SPN_ELF_SHT_SYMTAB) {
      return SPN_OK;
    }
  }
  return SPN_ERROR;
}

static spn_err_t scan_symbols(sp_mem_t mem, sp_io_seeking_reader_t* elf, const elf_shdr_t* symtab, const elf_shdr_t* strtab, sp_str_t prefix, bool* defined) {
  c8* names = sp_alloc(mem, strtab->sh_size);
  c8* symbols = sp_alloc(mem, symtab->sh_size);
  spn_try(read_at(elf, strtab->sh_offset, names, strtab->sh_size));
  spn_try(read_at(elf, symtab->sh_offset, symbols, symtab->sh_size));

  u64 count = symtab->sh_size / symtab->sh_entsize;
  sp_for(it, count) {
    elf_sym_t sym = sp_zero;
    sp_mem_copy(&sym, symbols + it * symtab->sh_entsize, sizeof(sym));
    if (sym.st_shndx == SPN_ELF_SHN_UNDEF) {
      continue;
    }
    if (sym.st_name >= strtab->sh_size) {
      return SPN_ERROR;
    }
    u32 len = 0;
    while (sym.st_name + len < strtab->sh_size && names[sym.st_name + len]) {
      len++;
    }
    if (sp_str_starts_with(sp_str(names + sym.st_name, len), prefix)) {
      *defined = true;
      return SPN_OK;
    }
  }
  return SPN_OK;
}

spn_err_t spn_elf_defines_prefix(sp_io_seeking_reader_t* elf, sp_str_t prefix, bool* defined) {
  *defined = false;

  elf_ehdr_t ehdr = sp_zero;
  spn_try(read_header(elf, &ehdr));

  elf_shdr_t symtab = sp_zero;
  spn_try(find_symtab(elf, &ehdr, &symtab));
  if (symtab.sh_entsize < sizeof(elf_sym_t)) {
    return SPN_ERROR;
  }
  elf_shdr_t strtab = sp_zero;
  spn_try(read_section(elf, &ehdr, symtab.sh_link, &strtab));

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_err_t err = scan_symbols(scratch.mem, elf, &symtab, &strtab, prefix, defined);
  sp_mem_end_scratch(scratch);
  return err;
}
