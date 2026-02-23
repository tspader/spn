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

sp_str_t     spn_cmake_gen_to_str(spn_cmake_gen_t gen);
void         spn_cmake(spn_build_ctx_t* build);
spn_cmake_t* spn_cmake_new(spn_build_ctx_t* build);
void         spn_cmake_set_generator(spn_cmake_t* cmake, spn_cmake_gen_t gen);
void         spn_cmake_add_define(spn_cmake_t* cmake, const c8* name, const c8* value);
void         spn_cmake_add_arg(spn_cmake_t* cmake, const c8* arg);
void         spn_cmake_configure(spn_cmake_t* cmake);
void         spn_cmake_build(spn_cmake_t* cmake);
void         spn_cmake_install(spn_cmake_t* cmake);
void         spn_cmake_run(spn_cmake_t* cmake);

#endif
