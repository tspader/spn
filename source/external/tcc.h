#ifndef SPN_EXTERNAL_TCC_H
#define SPN_EXTERNAL_TCC_H

#include "sp.h"
#include "spn.h"
#include "libtcc.h"

typedef TCCState spn_tcc_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_str_t error;
} spn_tcc_err_ctx_t;

spn_err_t spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file_path);
spn_err_t spn_tcc_register(spn_tcc_t* tcc);
s32 spn_tcc_backtrace(void* ud, void* pc, const c8* file, s32 line, const c8* fn, const c8* message);
void spn_tcc_on_build_script_compile_error(void* user_data, const c8* message);
void spn_tcc_list_fn(void* opaque, const c8* name, const void* value);

#endif
