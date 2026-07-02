#ifndef SPN_TOOLCHAIN_SELECT_H
#define SPN_TOOLCHAIN_SELECT_H

#include "error/types.h"
#include "toolchain/types.h"

typedef struct {
  sp_str_t build;
  sp_str_t script;
  spn_triple_t target;
  spn_triple_t host;
} spn_toolchain_query_t;

typedef struct {
  spn_toolchain_t* build;
  spn_toolchain_t* script;
  sp_da(spn_toolchain_t*) required;
} spn_toolchain_selection_t;

sp_str_t        spn_toolchain_script_default(void);
spn_triple_t    spn_toolchain_script_target(void);
bool            spn_toolchain_supports(spn_toolchain_t* toolchain, spn_triple_t target, spn_triple_t host);
spn_err_union_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_mem_t mem, spn_toolchain_selection_t* out);

#endif
