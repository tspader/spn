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

#define SPN_ELF_PT_INTERP 3
#define SPN_ELF_CLASS_64 2

static spn_err_t read_header(sp_io_seeking_reader_t* elf, elf_ehdr_t* ehdr) {
  s64 position = 0;
  u64 bytes = 0;
  if (sp_io_seeking_reader_seek(elf, 0, SP_IO_SEEK_SET, &position)) {
    return SPN_ERROR;
  }
  if (sp_io_read_all(elf->reader, ehdr, sizeof(*ehdr), &bytes) || bytes != sizeof(*ehdr)) {
    return SPN_ERROR;
  }
  if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' || ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
    return SPN_ERROR;
  }
  if (ehdr->e_ident[4] != SPN_ELF_CLASS_64) {
    return SPN_ERROR;
  }
  return SPN_OK;
}

spn_err_t spn_elf_interp(sp_mem_t mem, sp_io_seeking_reader_t* elf, sp_str_t* interp) {
  *interp = sp_str_lit("");
  s64 position = 0;
  u64 bytes = 0;

  elf_ehdr_t ehdr = sp_zero;
  spn_try(read_header(elf, &ehdr));

  u64 interp_offset = 0;
  u64 interp_size = 0;
  sp_for(it, ehdr.e_phnum) {
    elf_phdr_t phdr = sp_zero;
    if (sp_io_seeking_reader_seek(elf, (s64)(ehdr.e_phoff + it * ehdr.e_phentsize), SP_IO_SEEK_SET, &position)) {
      return SPN_ERROR;
    }
    if (sp_io_read_all(elf->reader, &phdr, sizeof(phdr), &bytes) || bytes != sizeof(phdr)) {
      return SPN_ERROR;
    }
    if (phdr.p_type == SPN_ELF_PT_INTERP && !interp_size) {
      interp_offset = phdr.p_offset;
      interp_size = phdr.p_filesz;
    }
  }

  if (!interp_size) {
    return SPN_OK;
  }

  c8* data = sp_alloc(mem, interp_size);
  if (sp_io_seeking_reader_seek(elf, (s64)interp_offset, SP_IO_SEEK_SET, &position)) {
    return SPN_ERROR;
  }
  if (sp_io_read_all(elf->reader, data, interp_size, &bytes) || bytes != interp_size) {
    return SPN_ERROR;
  }

  u32 len = 0;
  while (len < interp_size && data[len]) {
    len++;
  }
  *interp = sp_str(data, len);
  return SPN_OK;
}
