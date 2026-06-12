#include "ctx/types.h"

#include "external/tcc/tcc.h"
#include "external/tcc/error.h"
#include "external/tcc/backtrace.h"

typedef struct {
  const c8* symbol;
  void* fn;
} spn_tcc_symbol_t;

#define SPN_DEFINE_LIB_ENTRY(SYM) { .symbol = SP_MACRO_STR(SYM), .fn = SYM },
spn_libc_kind_t   spn_profile_get_libc(spn_profile_info_t* profile) { return SPN_LIBC_GNU; }

static spn_tcc_symbol_t spn_tcc_symbol_table[] = {
  SPN_DEFINE_LIB_ENTRY(spn_get_target)
  SPN_DEFINE_LIB_ENTRY(spn_get_dep)
  SPN_DEFINE_LIB_ENTRY(spn_get_subdir)
  SPN_DEFINE_LIB_ENTRY(spn_add_exe)
  SPN_DEFINE_LIB_ENTRY(spn_add_test)
  SPN_DEFINE_LIB_ENTRY(spn_add_include)
  SPN_DEFINE_LIB_ENTRY(spn_add_define)
  SPN_DEFINE_LIB_ENTRY(spn_add_system_dep)
  SPN_DEFINE_LIB_ENTRY(spn_target_add_source)
  SPN_DEFINE_LIB_ENTRY(spn_target_add_include)
  SPN_DEFINE_LIB_ENTRY(spn_target_add_define)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_file)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_file_ex)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_mem)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_mem_ex)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_dir)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_dir_ex)
  // SPN_DEFINE_LIB_ENTRY(spn_profile_get_libc)
  // SPN_DEFINE_LIB_ENTRY(spn_get_pkg)
  // SPN_DEFINE_LIB_ENTRY(spn_get_profile)
  // SPN_DEFINE_LIB_ENTRY(spn_get_linkage)
  // SPN_DEFINE_LIB_ENTRY(spn_get_target)
  // SPN_DEFINE_LIB_ENTRY(spn_get_dep)
  // SPN_DEFINE_LIB_ENTRY(spn_get_dir)
  // SPN_DEFINE_LIB_ENTRY(spn_get_subdir)
  // SPN_DEFINE_LIB_ENTRY(spn_add_include)
  // SPN_DEFINE_LIB_ENTRY(spn_add_define)
  // SPN_DEFINE_LIB_ENTRY(spn_add_system_dep)
  // SPN_DEFINE_LIB_ENTRY(spn_add_linkage)
  // SPN_DEFINE_LIB_ENTRY(spn_add_index)
  // SPN_DEFINE_LIB_ENTRY(spn_copy)
  // SPN_DEFINE_LIB_ENTRY(spn_log)
  // SPN_DEFINE_LIB_ENTRY(spn_profile_get_linkage)
  // SPN_DEFINE_LIB_ENTRY(spn_profile_get_standard)
  // SPN_DEFINE_LIB_ENTRY(spn_profile_get_mode)
  // SPN_DEFINE_LIB_ENTRY(spn_target_add_source)
  // SPN_DEFINE_LIB_ENTRY(spn_target_add_include)
  // SPN_DEFINE_LIB_ENTRY(spn_target_add_define)
  // SPN_DEFINE_LIB_ENTRY(spn_target_embed_file)
  // SPN_DEFINE_LIB_ENTRY(spn_target_embed_file_ex)
  // SPN_DEFINE_LIB_ENTRY(spn_target_embed_mem)
  // SPN_DEFINE_LIB_ENTRY(spn_target_embed_mem_ex)
  // SPN_DEFINE_LIB_ENTRY(spn_target_embed_dir)
  // SPN_DEFINE_LIB_ENTRY(spn_target_embed_dir_ex)
  // SPN_DEFINE_LIB_ENTRY(spn_make)
  // SPN_DEFINE_LIB_ENTRY(spn_make_new)
  // SPN_DEFINE_LIB_ENTRY(spn_make_add_target)
  // SPN_DEFINE_LIB_ENTRY(spn_make_run)
  // SPN_DEFINE_LIB_ENTRY(spn_autoconf)
  // SPN_DEFINE_LIB_ENTRY(spn_autoconf_new)
  // SPN_DEFINE_LIB_ENTRY(spn_autoconf_run)
  // SPN_DEFINE_LIB_ENTRY(spn_autoconf_add_flag)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_new)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_set_generator)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_add_define)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_add_arg)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_configure)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_build)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_install)
  // SPN_DEFINE_LIB_ENTRY(spn_cmake_run)
  // SPN_DEFINE_LIB_ENTRY(spn_add_node)
  // SPN_DEFINE_LIB_ENTRY(spn_node_add_input)
  // SPN_DEFINE_LIB_ENTRY(spn_node_add_output)
  // SPN_DEFINE_LIB_ENTRY(spn_node_link)
  // SPN_DEFINE_LIB_ENTRY(spn_node_set_fn)
  // SPN_DEFINE_LIB_ENTRY(spn_node_set_user_data)
  // SPN_DEFINE_LIB_ENTRY(spn_node_ctx_get_build)
  // SPN_DEFINE_LIB_ENTRY(spn_node_ctx_get_user_data)
  // SPN_DEFINE_LIB_ENTRY(spn_write_file)
};

spn_err_t spn_tcc_init(spn_tcc_t* tcc) {
  tcc->s = tcc_new();
  tcc_set_error_func(tcc->s, tcc, on_tcc_error);
  tcc_set_backtrace_func(tcc->s, tcc, on_tcc_backtrace);
  return SPN_OK;
}

void spn_tcc_set_runtime(spn_tcc_t* tcc, sp_str_t path) {
  tcc_set_lib_path(tcc->s, sp_str_to_cstr(path));
}

spn_err_t spn_tcc_add_sys_include(spn_tcc_t* tcc, sp_str_t path) {
  sp_mem_scratch_t scratch = sp_mem_begin_scratch();
  s32 result = tcc_add_sysinclude_path(tcc->s, sp_str_to_cstr(path));
  sp_mem_end_scratch(scratch);
  return result ? SPN_ERROR : SPN_OK;
}

spn_err_t spn_tcc_add_include(spn_tcc_t* tcc, sp_str_t path) {
  sp_mem_scratch_t scratch = sp_mem_begin_scratch();
  s32 result = tcc_add_include_path(tcc->s, sp_str_to_cstr(path));
  sp_mem_end_scratch(scratch);
  return result ? SPN_ERROR : SPN_OK;
}

void spn_tcc_add_library_path(spn_tcc_t* tcc, sp_str_t path) {
  sp_mem_scratch_t scratch = sp_mem_begin_scratch();
  tcc_add_library_path(tcc->s, sp_str_to_cstr(path));
  sp_mem_end_scratch(scratch);
}

void spn_tcc_define_symbol(spn_tcc_t* tcc, sp_str_t symbol, sp_str_t value) {
  sp_mem_scratch_t scratch = sp_mem_begin_scratch();
  tcc_define_symbol(tcc->s, sp_str_to_cstr(value), "");
  sp_mem_end_scratch(scratch);
}

spn_err_t spn_tcc_register(spn_tcc_t* tcc) {
  sp_carr_for(spn_tcc_symbol_table, it) {
    sp_try_as(tcc_add_symbol(tcc->s, spn_tcc_symbol_table[it].symbol, spn_tcc_symbol_table[it].fn), SPN_ERROR);
  }
  return SPN_OK;
}

spn_err_t spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file) {
  sp_mem_scratch_t scratch = sp_mem_begin_scratch();
  s32 error = tcc_add_file(tcc->s, sp_str_to_cstr(file));
  sp_mem_end_scratch(scratch);
  return error ? SPN_ERROR : SPN_OK;
}

void spn_tcc_list_fn(void* opaque, const c8* name, const void* value) {
  (void)value;
  sp_da(sp_str_t) syms = (sp_da(sp_str_t))opaque;
  sp_dyn_array_push(syms, sp_str_from_cstr(name));
}

void on_tcc_error(void* user_data, const c8* message) {
  spn_tcc_t* tcc = (spn_tcc_t*)user_data;
  tcc->error = sp_str_from_cstr(message);
}

s32 on_tcc_backtrace(void* ud, void* pc, const c8* file, s32 line, const c8* fn, const c8* message) {
  (void)ud;
  (void)pc;
  (void)file;
  (void)line;
  (void)fn;
  (void)message;
  return 0;
}
