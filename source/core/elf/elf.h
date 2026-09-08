#ifndef SPN_ELF_H
#define SPN_ELF_H

#include "sp.h"
#include "spn/core.h"

spn_err_t spn_elf_entry(sp_io_seeking_reader_t* elf, u64* entry);
spn_err_t spn_elf_interp(sp_mem_t mem, sp_io_seeking_reader_t* elf, sp_str_t* interp);
spn_err_t spn_elf_defines_prefix(sp_io_seeking_reader_t* elf, sp_str_t prefix, bool* defined);

#endif
