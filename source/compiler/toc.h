#ifndef SPN_COMPILER_TOC_H
#define SPN_COMPILER_TOC_H

#include "sp.h"
#include "spn.h"

#include "error/types.h"

typedef struct {
  sp_da(sp_str_t) symbols;
} spn_toc_t;

spn_err_union_t spn_toc_read(sp_mem_t mem, sp_str_t archive, spn_toc_t* toc);

#endif
