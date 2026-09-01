#ifndef spn_compiler_types_h
#define spn_compiler_types_h

#include "profile/types.h"
#include "toolchain/types.h"

typedef struct {
  sp_da(sp_str_t) compile;
  sp_da(sp_str_t) link;
} spn_cc_flags_t;

typedef enum {
  SPN_CC_OUTPUT_OBJECT,
  SPN_CC_OUTPUT_SHARED_LIB,
  SPN_CC_OUTPUT_STATIC_LIB,
  SPN_CC_OUTPUT_EXE,
  SPN_CC_OUTPUT_REACTOR,
} spn_cc_output_kind_t;

typedef struct {
  spn_cxx_standard_t standard;
  bool no_exceptions;
  bool no_rtti;
} spn_cxx_options_t;

typedef enum {
  SPN_AR_DRIVER_GNU,
  SPN_AR_DRIVER_MSVC,
} spn_ar_driver_t;

typedef enum {
  SPN_CC_CAP_TARGET_TRIPLE  = 1 << 0,
  SPN_CC_CAP_CLANG_FRONTEND = 1 << 1,
  SPN_CC_CAP_EXCLUDE_LIBS   = 1 << 2,
  SPN_CC_CAP_NOLIBC         = 1 << 3,
  SPN_CC_CAP_FREESTANDING   = 1 << 4,
  SPN_CC_CAP_LLVM_TRIPLE    = 1 << 5,
} spn_cc_cap_t;

typedef enum {
  SPN_CC_DEPFILE_NONE,
  SPN_CC_DEPFILE_OPTIONAL,
  SPN_CC_DEPFILE_REQUIRED,
} spn_cc_depfile_t;

typedef u32 spn_cc_cap_set_t;

typedef struct {
  sp_str_t name;
  spn_cc_driver_t driver;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t cxx;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  spn_ar_driver_t archiver_driver;
} spn_cc_toolchain_t;

typedef struct {
  spn_lang_t lang;
  sp_da(spn_path_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) args;
  spn_cxx_options_t cxx;
  bool pic;
  spn_os_version_t min_os;
} spn_cc_compile_t;

typedef struct {
  spn_path_t source;
  spn_path_t output;
  spn_path_t depfile;
} spn_cc_compile_files_t;

typedef struct {
  spn_path_t path;
  sp_da(sp_str_t) symbols;
} spn_cc_exports_t;

typedef struct {
  spn_lang_t lang;
  spn_cc_output_kind_t kind;
  sp_da(sp_str_t) libs;
  sp_da(sp_str_t) private_libs;
  sp_da(sp_str_t) system_libs;
  sp_da(spn_path_t) lib_dirs;
  sp_da(sp_str_t) frameworks;
  spn_os_version_t min_os;
  spn_win_subsystem_t subsystem;
  bool rpath;
} spn_cc_link_t;

typedef struct {
  spn_path_t output;
  sp_da(spn_path_t) objects;
  sp_da(spn_path_t) whole_archives;
  spn_cc_exports_t exports;
} spn_cc_link_files_t;

typedef struct {
  spn_path_t output;
  sp_da(spn_path_t) objects;
} spn_cc_archive_files_t;

typedef enum {
  SPN_CC_EXPORTS_VERSION_SCRIPT,
  SPN_CC_EXPORTS_SYMBOL_LIST,
  SPN_CC_EXPORTS_DEF,
  SPN_CC_EXPORTS_WASM,
} spn_cc_exports_format_t;

typedef struct {
  spn_arg_t program;
  sp_da(spn_arg_t) args;
  u32 launcher;
  spn_path_t cwd;
} spn_invocation_t;

typedef struct {
  sp_str_t content;
  spn_invocation_t invocation;
} spn_rsp_t;

#endif
