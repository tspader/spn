#ifndef SPN_CC_H
#define SPN_CC_H

#include "sp.h"
#include "sp/elf.h"
#include "spn.h"
#include "target/types.h"
#include "external/tcc.h"

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
  sp_str_t symbol;
  u64 size;
  spn_embed_types_t types;
} spn_cc_embed_t;

typedef struct {
  sp_elf_t* elf;
  sp_da(spn_cc_embed_t) entries;
} spn_cc_embed_ctx_t;

typedef struct {
  sp_str_t output;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) libs;
  sp_da(sp_str_t) lib_dirs;
  sp_da(sp_str_t) rpath;

  spn_target_kind_t kind;
  union {
    spn_cc_executable_t exe;
    spn_cc_shared_lib_t shared_lib;
    spn_cc_static_lib_t static_lib;
  };
  spn_cc_t* cc;
} spn_cc_target_t;

struct spn_cc {
  spn_profile_t* profile;
  struct {
    spn_cc_kind_t kind;
    sp_str_t exe;
  } compiler;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_linkage_t linkage;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_str_t dir;
  sp_da(spn_cc_target_t) targets;
  sp_ps_config_t config;
};

void             spn_cc_set_profile(spn_cc_t* cc, spn_profile_t* profile);
void             spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_include(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_relative_include(spn_cc_t* cc, sp_str_t dir);
void             spn_cc_add_define(spn_cc_t* cc, sp_str_t var);
void             spn_cc_add_pkg(spn_cc_t* cc, spn_pkg_t* pkg);
spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_target_kind_t kind, sp_str_t name);
void             spn_cc_target_add_relative_source(spn_cc_target_t* cc, sp_str_t path);
void             spn_cc_target_add_absolute_source(spn_cc_target_t* cc, sp_str_t path);
void             spn_cc_target_add_define(spn_cc_target_t* cc, sp_str_t var);
void             spn_cc_target_add_relative_include(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_absolute_include(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_lib(spn_cc_target_t* cc, sp_str_t lib);
void             spn_cc_target_add_lib_dir(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_rpath(spn_cc_target_t* cc, sp_str_t dir);
void             spn_cc_target_add_dep(spn_cc_target_t* target, spn_pkg_unit_t* dep);
void             spn_cc_target_to_tcc(spn_cc_t* cc, spn_cc_target_t* target, spn_tcc_t* tcc);
void             spn_cc_to_ps(spn_cc_t* cc, sp_ps_config_t* ps);
void             spn_cc_target_to_ps(spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps);
sp_str_t         spn_cc_symbol_from_embedded_file(sp_str_t file_path);
void             spn_cc_embed_ctx_init(spn_cc_embed_ctx_t* ctx);
spn_err_t        spn_cc_embed_ctx_add(spn_cc_embed_ctx_t* ctx, sp_io_reader_t reader, sp_str_t symbol, sp_str_t data_type, sp_str_t size_type);
spn_err_t        spn_cc_embed_ctx_write(spn_cc_embed_ctx_t* ctx, sp_str_t object, sp_str_t header);

#endif
