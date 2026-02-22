#ifndef SPN_CMAKE_H
#define SPN_CMAKE_H

#include "sp.h"
#include "spn.h"

typedef struct {
  sp_str_t name;
  sp_str_t value;
} spn_cmake_define_t;

struct spn_cmake {
  spn_build_ctx_t* build;
  spn_cmake_gen_t generator;
  sp_da(spn_cmake_define_t) defines;
  sp_da(sp_str_t) args;
};

#endif
