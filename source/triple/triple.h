#ifndef SPN_TRIPLE_TRIPLE_H
#define SPN_TRIPLE_TRIPLE_H

#include "sp.h"
#include "spn/core.h"

spn_triple_t spn_triple_from_str(sp_str_t str);
spn_err_t spn_triple_parse(sp_str_t str, spn_triple_t* triple);
sp_str_t spn_triple_to_str(sp_mem_t mem, spn_triple_t triple);
spn_triple_t spn_triple_host(void);
spn_triple_t spn_triple_merge(spn_triple_t base, spn_triple_t partial);
bool spn_triple_match(spn_triple_t entry, spn_triple_t target);
sp_str_t spn_triple_to_cc_target(sp_mem_t mem, spn_triple_t triple);
sp_str_t spn_triple_to_autoconf(sp_mem_t mem, spn_triple_t triple);
sp_str_t spn_os_to_cmake_system_name(spn_os_t os);
sp_str_t spn_triple_lib_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name, sp_os_lib_kind_t kind);
sp_str_t spn_triple_exe_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name);

#endif
