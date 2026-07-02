#ifndef SPN_EXTERNAL_TCC_TYPES_H
#define SPN_EXTERNAL_TCC_TYPES_H

#include "sp.h"
#include "libtcc.h"

typedef struct {
  TCCState* s;
  sp_mem_t mem;
  sp_str_t error;
} spn_tcc_t;

#endif
