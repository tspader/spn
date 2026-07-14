#ifndef spn_compiler_types_h
#define spn_compiler_types_h

#include "sp.h"
#include "spn.h"

#include "profile/types.h"
#include "toolchain/types.h"

typedef struct {
  sp_da(sp_str_t) compile;
  sp_da(sp_str_t) link;
} spn_cc_flags_t;

typedef enum {
  SPN_CC_FEATURE_COMPILE,
  SPN_CC_FEATURE_LINK_EXE,
  SPN_CC_FEATURE_LINK_SHARED,
  SPN_CC_FEATURE_LINK_REACTOR,
  SPN_CC_FEATURE_ARCHIVE,
  SPN_CC_FEATURE_FRAMEWORKS,
} spn_cc_feature_t;

typedef enum {
  SPN_CC_OUTPUT_OBJECT,
  SPN_CC_OUTPUT_SHARED_LIB,
  SPN_CC_OUTPUT_STATIC_LIB,
  SPN_CC_OUTPUT_EXE,
  SPN_CC_OUTPUT_REACTOR,
} spn_cc_output_kind_t;

typedef enum {
  SPN_SYMBOL_VISIBILITY_DEFAULT,
  SPN_SYMBOL_VISIBILITY_HIDDEN,
} spn_symbol_visibility_t;

typedef struct {
  spn_cxx_standard_t standard;
  bool no_exceptions;
  bool no_rtti;
} spn_cxx_options_t;

typedef enum {
  SPN_AR_DRIVER_GNU,
  SPN_AR_DRIVER_MSVC,
} spn_ar_driver_t;

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
  sp_str_t source;
  sp_str_t output;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) args;
  spn_cxx_options_t cxx;
  spn_symbol_visibility_t visibility;
  bool pic;
  spn_os_version_t min_os;
} spn_cc_compile_t;

typedef struct {
  spn_lang_t lang;
  spn_cc_output_kind_t kind;
  sp_str_t output;
  sp_da(sp_str_t) objects;
  sp_da(sp_str_t) args;
  sp_da(sp_str_t) libs;
  sp_da(sp_str_t) system_libs;
  sp_da(sp_str_t) hidden_libs;
  sp_da(sp_str_t) lib_dirs;
  sp_da(sp_str_t) rpath;
  sp_da(sp_str_t) frameworks;
  spn_os_version_t min_os;
  spn_win_subsystem_t subsystem;
} spn_cc_link_t;

typedef struct {
  sp_str_t output;
  sp_da(sp_str_t) objects;
  sp_da(sp_str_t) args;
} spn_cc_archive_t;

#endif
