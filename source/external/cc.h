#ifndef SPN_CC_H
#define SPN_CC_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"
#include "compiler/types.h"
#include "target/types.h"
#include "toolchain/types.h"

#include "external/obj.h"

typedef enum {
  SPN_CC_OUTPUT_OBJECT,
  SPN_CC_OUTPUT_SHARED_LIB,
  SPN_CC_OUTPUT_STATIC_LIB,
  SPN_CC_OUTPUT_EXE,
  SPN_CC_OUTPUT_WASM,
} spn_cc_output_kind_t;

typedef struct {
  sp_str_t name;
} spn_cc_executable_t;

typedef struct {
  sp_str_t name;
} spn_cc_shared_lib_t;

typedef struct {
  sp_str_t name;
} spn_cc_static_lib_t;

typedef struct {
  sp_str_t build;
  sp_str_t profile;
  sp_str_t output;
} spn_cc_target_paths_t;

typedef struct {
  sp_str_t path;
  sp_str_t symbol;
  u64 size;
  spn_embed_types_t types;
} spn_cc_embed_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  spn_obj_builder_t obj;
  sp_da(spn_cc_embed_t) entries;
} spn_cc_embed_ctx_t;

typedef struct {
  sp_str_t output;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) flags;
  sp_da(sp_str_t) libs;
  sp_da(sp_str_t) system_libs;
  sp_da(sp_str_t) lib_dirs;
  sp_da(sp_str_t) rpath;

  spn_cc_output_kind_t kind;
  spn_lang_t lang;
  spn_cxx_options_t cxx;
  union {
    spn_cc_executable_t exe;
    spn_cc_shared_lib_t shared_lib;
    spn_cc_static_lib_t static_lib;
  };
  spn_cc_t* cc;
} spn_cc_target_t;

struct spn_cc {
  sp_mem_t mem;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t cxx;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  spn_cc_driver_t driver;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_cc_flags_t flags;

  struct {
    sp_str_t runtime;
    sp_str_t include;
  } spn;

  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_str_t dir;
  sp_da(spn_cc_target_t) targets;
  sp_ps_config_t config;
};

void             spn_cc_init(spn_cc_t* cc, sp_mem_t mem);
void             spn_cc_add_runtime(spn_cc_t* cc, sp_str_t runtime, sp_str_t include);
void             spn_cc_set_toolchain(spn_cc_t* cc, spn_toolchain_unit_t* toolchain);
void             spn_cc_set_profile(spn_cc_t* cc, spn_profile_info_t profile);
void             spn_cc_set_flags(spn_cc_t* cc, spn_cc_flags_t flags);
void             spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_include(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_relative_include(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_define(spn_cc_t* cc, sp_str_t var);
void             spn_cc_add_pkg(spn_cc_t* cc, spn_pkg_unit_t* pkg);
void             spn_cc_target_add_info(spn_cc_target_t* target, spn_pkg_unit_t* pkg, spn_target_info_t* info);
spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_output_kind_t kind, sp_str_t name);
void             spn_cc_target_set_lang(spn_cc_target_t* cc, spn_lang_t lang);
void             spn_cc_target_add_relative_source(spn_cc_target_t* cc, sp_str_t path);
void             spn_cc_target_add_absolute_source(spn_cc_target_t* cc, sp_str_t path);
void             spn_cc_target_add_define(spn_cc_target_t* cc, sp_str_t var);
void             spn_cc_target_add_flag(spn_cc_target_t* cc, sp_str_t flag);
void             spn_cc_target_add_relative_include(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_absolute_include(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_lib(spn_cc_target_t* cc, sp_str_t lib);
void             spn_cc_target_add_system_lib(spn_cc_target_t* cc, sp_str_t name);
void             spn_cc_target_add_lib_dir(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_rpath(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_dep(spn_cc_target_t* target, spn_pkg_unit_t* dep);
void             spn_cc_to_ps(sp_mem_t mem, spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps);
void             spn_cc_target_to_ps(sp_mem_t mem, spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps);
sp_str_t         spn_cc_symbol_from_embedded_file(sp_mem_t mem, sp_str_t file_path);
void             spn_cc_embed_ctx_init(spn_cc_embed_ctx_t* ctx, sp_mem_t mem, spn_os_t target_os, spn_arch_t target_arch);
void             spn_cc_embed_ctx_free(spn_cc_embed_ctx_t* ctx);
spn_err_t        spn_cc_embed_ctx_add(spn_cc_embed_ctx_t* ctx, sp_mem_buffer_t data, sp_str_t symbol, sp_str_t path, sp_str_t data_type, sp_str_t size_type);
spn_err_t        spn_cc_embed_ctx_write(spn_cc_embed_ctx_t* ctx, sp_str_t object, sp_str_t header);

#endif
