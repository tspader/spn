#ifndef SPN_EXTERNAL_TCC_TYPES_H
#define SPN_EXTERNAL_TCC_TYPES_H

#include "sp.h"
#include "libtcc.h"

typedef struct {
  sp_mem_arena_t* arena;
  sp_str_t error;
} spn_tcc_err_ctx_t;

typedef struct {
  TCCState* s;
  sp_mem_arena_t* arena;
  sp_str_t error;
} spn_tcc_t;

#endif
