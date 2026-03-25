#include "ctx/types.h"
#include "tcc.h"

#include "sp/macro.h"

typedef struct {
  const c8* symbol;
  void* fn;
} spn_tcc_symbol_t;

#define SPN_DEFINE_LIB_ENTRY(SYM) { .symbol = SP_MACRO_STR(SYM), .fn = SYM },

static spn_tcc_symbol_t spn_tcc_symbol_table[] = {
  SPN_DEFINE_LIB_ENTRY(spn_get_pkg)
  SPN_DEFINE_LIB_ENTRY(spn_get_profile)
  SPN_DEFINE_LIB_ENTRY(spn_get_linkage)
  SPN_DEFINE_LIB_ENTRY(spn_get_target)
  SPN_DEFINE_LIB_ENTRY(spn_get_dep)
  SPN_DEFINE_LIB_ENTRY(spn_get_dir)
  SPN_DEFINE_LIB_ENTRY(spn_get_subdir)
  SPN_DEFINE_LIB_ENTRY(spn_add_exe)
  SPN_DEFINE_LIB_ENTRY(spn_add_test)
  SPN_DEFINE_LIB_ENTRY(spn_add_include)
  SPN_DEFINE_LIB_ENTRY(spn_add_define)
  SPN_DEFINE_LIB_ENTRY(spn_add_system_dep)
  SPN_DEFINE_LIB_ENTRY(spn_add_linkage)
  SPN_DEFINE_LIB_ENTRY(spn_add_index)
  SPN_DEFINE_LIB_ENTRY(spn_copy)
  SPN_DEFINE_LIB_ENTRY(spn_log)
  SPN_DEFINE_LIB_ENTRY(spn_profile_get_cc)
  SPN_DEFINE_LIB_ENTRY(spn_profile_get_cc_exe)
  SPN_DEFINE_LIB_ENTRY(spn_profile_get_linkage)
  SPN_DEFINE_LIB_ENTRY(spn_profile_get_libc)
  SPN_DEFINE_LIB_ENTRY(spn_profile_get_standard)
  SPN_DEFINE_LIB_ENTRY(spn_profile_get_mode)
  SPN_DEFINE_LIB_ENTRY(spn_target_add_source)
  SPN_DEFINE_LIB_ENTRY(spn_target_add_include)
  SPN_DEFINE_LIB_ENTRY(spn_target_add_define)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_file)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_file_ex)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_mem)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_mem_ex)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_dir)
  SPN_DEFINE_LIB_ENTRY(spn_target_embed_dir_ex)
  SPN_DEFINE_LIB_ENTRY(spn_make)
  SPN_DEFINE_LIB_ENTRY(spn_make_new)
  SPN_DEFINE_LIB_ENTRY(spn_make_add_target)
  SPN_DEFINE_LIB_ENTRY(spn_make_run)
  SPN_DEFINE_LIB_ENTRY(spn_autoconf)
  SPN_DEFINE_LIB_ENTRY(spn_autoconf_new)
  SPN_DEFINE_LIB_ENTRY(spn_autoconf_run)
  SPN_DEFINE_LIB_ENTRY(spn_autoconf_add_flag)
  SPN_DEFINE_LIB_ENTRY(spn_cmake)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_new)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_set_generator)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_add_define)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_add_arg)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_configure)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_build)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_install)
  SPN_DEFINE_LIB_ENTRY(spn_cmake_run)
  SPN_DEFINE_LIB_ENTRY(spn_add_node)
  SPN_DEFINE_LIB_ENTRY(spn_node_add_input)
  SPN_DEFINE_LIB_ENTRY(spn_node_add_output)
  SPN_DEFINE_LIB_ENTRY(spn_node_link)
  SPN_DEFINE_LIB_ENTRY(spn_node_set_fn)
  SPN_DEFINE_LIB_ENTRY(spn_node_set_user_data)
  SPN_DEFINE_LIB_ENTRY(spn_node_ctx_get_build)
  SPN_DEFINE_LIB_ENTRY(spn_node_ctx_get_user_data)
  SPN_DEFINE_LIB_ENTRY(spn_write_file)
};

// @spader What does "prepare script" mean? This is just setting up the context?
spn_err_t spn_tcc_prepare_script(spn_tcc_t* tcc, spn_tcc_err_ctx_t* error_context) {
  tcc_set_error_func(tcc, error_context, spn_tcc_on_build_script_compile_error);
  tcc_set_backtrace_func(tcc, error_context, spn_tcc_backtrace);
  tcc_set_lib_path(tcc, sp_str_to_cstr(spn.paths.runtime));
  tcc_add_sysinclude_path(tcc, sp_str_to_cstr(spn.paths.include));
  tcc_add_include_path(tcc, sp_str_to_cstr(spn.paths.include));
  tcc_set_options(tcc, "-gdwarf -Wall -Werror");
  spn_try_as(tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY), SPN_ERROR);
  spn_try_as(tcc_add_include_path(tcc, sp_str_to_cstr(spn.paths.include)), SPN_ERROR);
  tcc_define_symbol(tcc, "SPN", "");
  sp_try(spn_tcc_register(tcc));
  return SPN_OK;
}

spn_err_t spn_tcc_register(spn_tcc_t* tcc) {
  sp_carr_for(spn_tcc_symbol_table, it) {
    sp_try_as(tcc_add_symbol(tcc, spn_tcc_symbol_table[it].symbol, spn_tcc_symbol_table[it].fn), SPN_ERROR);
  }
  return SPN_OK;
}

spn_err_t spn_tcc_add_file(spn_tcc_t* tcc, sp_str_t file_path) {
  sp_try_as(tcc_add_file(tcc, sp_str_to_cstr(file_path)), SPN_ERROR);
  return SPN_OK;
}

s32 spn_tcc_backtrace(void* ud, void* pc, const c8* file, s32 line, const c8* fn, const c8* message) {
  (void)ud;
  (void)pc;
  (void)file;
  (void)line;
  (void)fn;
  (void)message;
  return 0;
}

void spn_tcc_on_build_script_compile_error(void* user_data, const c8* message) {
  spn_tcc_err_ctx_t* e = (spn_tcc_err_ctx_t*)user_data;

  sp_context_push_allocator(sp_mem_arena_as_allocator(e->arena));
  e->error = sp_str_from_cstr(message);
  sp_context_pop();
}

void spn_tcc_list_fn(void* opaque, const c8* name, const void* value) {
  (void)value;
  sp_da(sp_str_t) syms = (sp_da(sp_str_t))opaque;
  sp_dyn_array_push(syms, sp_str_from_cstr(name));
}
