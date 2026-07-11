#ifndef spn_compiler_types_h
#define spn_compiler_types_h

#include "sp.h"

typedef struct {
  sp_da(sp_str_t) compile;
  sp_da(sp_str_t) link;
} spn_cc_flags_t;

#endif
