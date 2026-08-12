#ifndef CODEGEN_H
#define CODEGEN_H

#include "sp.h"

typedef struct {
  sp_str_t schema;
  sp_str_t out;
  sp_str_t templates;
} codegen_paths_t;

typedef void (*codegen_log_fn_t)(void* user, sp_str_t message);

sp_str_t codegen_run(sp_mem_t mem, codegen_paths_t paths, codegen_log_fn_t log, void* user);

#endif
