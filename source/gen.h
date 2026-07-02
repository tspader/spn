#ifndef SPN_GEN_H
#define SPN_GEN_H

#include "sp.h"
#include "spn.h"

#include "toolchain/types.h"

typedef enum {
  SPN_GEN_KIND_RAW,
  SPN_GEN_KIND_SHELL,
  SPN_GEN_KIND_MAKE,
  SPN_GEN_KIND_CMAKE,
  SPN_GEN_KIND_PKGCONFIG,
} spn_gen_kind_t;

typedef enum {
  SPN_GEN_NONE,
  SPN_GEN_INCLUDE,
  SPN_GEN_LIB_INCLUDE,
  SPN_GEN_LIBS,
  SPN_GEN_SYSTEM_LIBS,
  SPN_GEN_RPATH,
  SPN_GEN_DEFINE,
  SPN_GEN_ALL,
} spn_gen_entry_t;

typedef struct {
  spn_gen_kind_t kind;
  spn_cc_kind_t compiler;
  sp_str_t file_name;
  sp_str_t output;

  sp_str_t include;
  sp_str_t lib_include;
  sp_str_t libs;
  sp_str_t system_libs;
  sp_str_t rpath;
} spn_generator_t;

typedef struct {
  spn_gen_entry_t kind;
  spn_cc_driver_t driver;
} spn_gen_format_context_t;

spn_gen_kind_t spn_gen_kind_from_str(sp_str_t str);
spn_gen_entry_t spn_gen_entry_from_str(sp_str_t str);
sp_str_t spn_cc_kind_to_executable(spn_cc_kind_t compiler);
sp_str_t spn_cc_c_standard_to_switch(spn_c_standard_t standard);
sp_str_t spn_cc_lib_kind_to_switch(spn_linkage_t kind);
sp_str_t spn_cc_build_mode_to_switch(spn_build_mode_t mode);
sp_str_t spn_gen_format_entry(sp_mem_t mem, sp_str_t entry, spn_gen_entry_t kind, spn_cc_driver_t driver);
sp_str_t spn_gen_format_entry_kernel(sp_str_map_context_t* context);

#endif
