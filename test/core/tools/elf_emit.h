#ifndef SPN_TEST_ELF_EMIT_H
#define SPN_TEST_ELF_EMIT_H

#include "sp.h"

#define ELF_EMIT_MAX_SYMBOLS 4

typedef struct {
  const c8* interp;
  u64 entry;
  const c8* symbols [ELF_EMIT_MAX_SYMBOLS];
  const c8* undefined [ELF_EMIT_MAX_SYMBOLS];
  bool bad_magic;
  bool elf32;
  bool load_first;
  bool truncated;
} elf_spec_t;

sp_str_t             elf_emit(sp_mem_t mem, const elf_spec_t* spec);
sp_io_seeking_reader_t elf_reader(sp_io_reader_t* backing, sp_str_t elf);

#endif
