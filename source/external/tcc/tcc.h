#ifndef SPN_EXTERNAL_TCC_H
#define SPN_EXTERNAL_TCC_H

#include "sp.h"
#include "error/types.h"
#include "external/tcc/types.h"

spn_err_t spn_tcc_prepare_script(spn_tcc_t* tcc, spn_tcc_err_ctx_t* error_context);
spn_err_t spn_tcc_init(spn_tcc_t* tcc);
spn_err_t spn_tcc_register(spn_tcc_t* tcc);
void      spn_tcc_list_fn(void* opaque, const c8* name, const void* value);
spn_err_t spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file);
void      spn_tcc_set_runtime(spn_tcc_t* tcc, sp_str_t path);
spn_err_t spn_tcc_add_sys_include(spn_tcc_t* tcc, sp_str_t path);
spn_err_t spn_tcc_add_include(spn_tcc_t* tcc, sp_str_t path);
void spn_tcc_add_library_path(spn_tcc_t* tcc, sp_str_t path);
void spn_tcc_add_lib(spn_tcc_t* tcc, sp_str_t lib);
void spn_tcc_define_symbol(spn_tcc_t* tcc, sp_str_t symbol, sp_str_t value);

#endif
