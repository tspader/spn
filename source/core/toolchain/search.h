#ifndef SPN_TOOLCHAIN_SEARCH_H
#define SPN_TOOLCHAIN_SEARCH_H

#include "sp.h"

#if defined(SP_WIN32)
  #define SPN_SEARCH_PATH_SEP ';'
#else
  #define SPN_SEARCH_PATH_SEP ':'
#endif

sp_da(sp_str_t) spn_search_split_path(sp_mem_t mem, sp_str_t path);
sp_str_t        spn_search_program(sp_mem_t mem, sp_str_t cwd, sp_str_t program, sp_da(sp_str_t) dirs);

#endif
