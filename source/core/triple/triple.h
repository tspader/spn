#ifndef SPN_TRIPLE_TRIPLE_H
#define SPN_TRIPLE_TRIPLE_H

#include "sp.h"
#include "spn/core.h"

typedef enum {
  SPN_TRIPLE_ENTRY_OK,
  SPN_TRIPLE_ENTRY_MISSING_ARCH,
  SPN_TRIPLE_ENTRY_MISSING_OS,
  SPN_TRIPLE_ENTRY_MISSING_ABI,
  SPN_TRIPLE_ENTRY_FOREIGN_ARCH,
  SPN_TRIPLE_ENTRY_FOREIGN_ABI,
} spn_triple_entry_t;

spn_err_t spn_triple_parse(sp_str_t str, spn_triple_t* triple);
spn_err_t spn_triple_parse_host(sp_str_t str, spn_triple_t* triple);
sp_str_t spn_triple_to_str(sp_mem_t mem, spn_triple_t triple);
spn_triple_t spn_triple_host();
spn_err_t spn_elf_interp(sp_mem_t mem, sp_io_seeking_reader_t* elf, sp_str_t* interp);
spn_abi_t spn_abi_from_interp(sp_str_t interp);
spn_abi_t spn_host_libc(sp_mem_t mem, sp_io_seeking_reader_t* elf);
u32 spn_os_abis(spn_os_t os, const spn_abi_t** abis);
u32 spn_os_archs(spn_os_t os, const spn_arch_t** archs);
bool spn_os_dynamic(spn_os_t os);
spn_triple_entry_t spn_triple_entry(spn_triple_t partial, spn_triple_t* full);
spn_triple_t spn_triple_merge(spn_triple_t base, spn_triple_t partial);
bool spn_triple_match(spn_triple_t entry, spn_triple_t target);
bool spn_triple_equal(spn_triple_t a, spn_triple_t b);
sp_str_t spn_triple_lib_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name, sp_os_lib_kind_t kind);
sp_str_t spn_triple_exe_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name);

#endif
