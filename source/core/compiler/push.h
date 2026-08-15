#ifndef spn_compiler_push_h
#define spn_compiler_push_h

#include "compiler/types.h"
#include "sp.h"
#include "spn/core.h"

void spn_cc_push(sp_mem_t mem, spn_invocation_t* invocation, spn_arg_t arg);
void spn_cc_push_c(sp_mem_t mem, spn_invocation_t* invocation, const c8* value);
void spn_cc_push_str(sp_mem_t mem, spn_invocation_t* invocation, sp_str_t value);
void spn_cc_push_fmt(sp_mem_t mem, spn_invocation_t* invocation, const c8* fmt, ...);
void spn_cc_push_path(sp_mem_t mem, spn_invocation_t* invocation, spn_path_t path);
void spn_cc_push_glued(sp_mem_t mem, spn_invocation_t* invocation, const c8* prefix, spn_path_t path);
void spn_cc_push_strs(sp_mem_t mem, spn_invocation_t* invocation, sp_da(sp_str_t) values);
void spn_cc_push_paths(sp_mem_t mem, spn_invocation_t* invocation, sp_da(spn_path_t) paths);
void spn_cc_push_args(sp_mem_t mem, spn_invocation_t* invocation, sp_da(spn_arg_t) args);

#endif
