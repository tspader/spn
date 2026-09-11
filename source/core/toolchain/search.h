#ifndef SPN_TOOLCHAIN_SEARCH_H
#define SPN_TOOLCHAIN_SEARCH_H

#include "sp.h"
#include "spn/core.h"
#include "paths/types.h"

#define SPN_SEARCH_MAX_SUFFIX 2

typedef struct {
  c8 sep;
  sp_fs_path_kind_t kind;
  const c8* suffix [SPN_SEARCH_MAX_SUFFIX];
} spn_search_rules_t;

spn_search_rules_t spn_search_rules(spn_os_t os);
sp_da(sp_str_t)    spn_search_dirs(spn_search_rules_t rules, sp_mem_t mem, sp_str_t path);
sp_str_t           spn_search_program(spn_search_rules_t rules, sp_mem_t mem, const spn_path_roots_t* roots, spn_arg_t program, sp_da(sp_str_t) dirs);
sp_str_t           spn_search_prepend(spn_search_rules_t rules, sp_mem_t mem, sp_str_t dir, sp_str_t path);

#endif
