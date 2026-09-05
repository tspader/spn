#include "elf_emit.h"

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

#define ELF_PT_LOAD 1
#define ELF_PT_INTERP 3

sp_str_t elf_emit(sp_mem_t mem, const elf_spec_t* spec) {
  u32 num_phdrs = (spec->load_first ? 1 : 0) + (spec->interp ? 1 : 0);
  u64 phoff = sizeof(elf_ehdr_t);
  u64 interp_off = phoff + num_phdrs * sizeof(elf_phdr_t);
  sp_str_t interp = spec->interp ? sp_cstr_as_str(spec->interp) : (sp_str_t) sp_zero;
  u64 size = interp_off + interp.len + 1;

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

  return sp_str(bytes, (u32)size);
}

sp_io_seeking_reader_t elf_reader(sp_io_reader_t* backing, sp_str_t elf) {
  sp_io_seeking_reader_t reader = sp_zero;
  sp_io_seeking_reader_from_mem(&reader, backing, elf.data, elf.len);
  return reader;
}
