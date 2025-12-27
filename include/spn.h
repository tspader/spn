#ifndef SPN_SPN_H
#define SPN_SPN_H

#ifndef SP_SP_H
  #include <stdint.h>
  // #include <stdbool.h>
  // #include <stdlib.h>
  // #include <string.h>

  typedef int8_t   s8;
  typedef int16_t  s16;
  typedef int32_t  s32;
  typedef int64_t  s64;
  typedef uint8_t  u8;
  typedef uint16_t u16;
  typedef uint32_t u32;
  typedef uint64_t u64;
  typedef float    f32;
  typedef double   f64;
  typedef char     c8;
#endif

typedef enum {
  SPN_DEP_BUILD_MODE_DEBUG = 0,
  SPN_DEP_BUILD_MODE_RELEASE = 1,
} spn_build_mode_t;

typedef enum {
  SPN_LIB_KIND_NONE = 0,
  SPN_LIB_KIND_SHARED = 1,
  SPN_LIB_KIND_STATIC = 2,
  SPN_LIB_KIND_SOURCE = 3,
} spn_pkg_linkage_t;

typedef enum {
  SPN_LIBC_GNU = 0,
  SPN_LIBC_MUSL = 1,
  SPN_LIBC_COSMOPOLITAN = 2,
  SPN_LIBC_CUSTOM = 3,
} spn_libc_kind_t;

typedef enum {
  SPN_DIR_NONE = 0,
  SPN_DIR_CACHE = 1,
  SPN_DIR_STORE = 2,
  SPN_DIR_INCLUDE = 3,
  SPN_DIR_VENDOR = 4,
  SPN_DIR_LIB = 5,
  SPN_DIR_SOURCE = 6,
  SPN_DIR_WORK = 7,
} spn_dir_kind_t;

typedef enum {
  SPN_C11,
  SPN_C99,
  SPN_C89,
} spn_c_standard_t;

typedef enum {
  SPN_CC_NONE,
  SPN_CC_GCC,
  SPN_CC_CLANG,
  SPN_CC_MUSL_GCC,
  SPN_CC_TCC,
  SPN_CC_CUSTOM,
} spn_cc_kind_t;

typedef enum {
  SPN_CMAKE_GEN_DEFAULT,
  SPN_CMAKE_GEN_UNIX_MAKEFILES,
  SPN_CMAKE_GEN_NINJA,
  SPN_CMAKE_GEN_XCODE,
  SPN_CMAKE_GEN_MSVC,
  SPN_CMAKE_GEN_MINGW,
} spn_cmake_gen_t;

typedef struct spn_config spn_config_t;
typedef struct spn_pkg spn_pkg_t;
typedef struct spn_build_ctx spn_build_ctx_t;
typedef struct spn_bin_ctx spn_bin_ctx_t;
typedef struct spn_autoconf spn_autoconf_t;
typedef struct spn_make spn_make_t;
typedef struct spn_cmake spn_cmake_t;
typedef struct spn_cc spn_cc_t;

typedef void(*spn_config_fn_t)(spn_config_t*);
typedef void(*spn_build_fn_t)(spn_build_ctx_t*);

void            spn_make(spn_build_ctx_t* build);
spn_make_t*     spn_make_new(spn_build_ctx_t* build);
void            spn_make_add_target(spn_make_t* make, const c8* target);
void            spn_make_run(spn_make_t* make);
void            spn_autoconf(spn_build_ctx_t* build);
spn_autoconf_t* spn_autoconf_new(spn_build_ctx_t* build);
void            spn_autoconf_run(spn_autoconf_t* autoconf);
void            spn_autoconf_add_flag(spn_autoconf_t* autoconf, const c8* flag);
void            spn_cmake(spn_build_ctx_t* build);
spn_cmake_t*    spn_cmake_new(spn_build_ctx_t* build);
void            spn_cmake_set_generator(spn_cmake_t* cmake, spn_cmake_gen_t gen);
void            spn_cmake_add_define(spn_cmake_t* cmake, const c8* name, const c8* value);
void            spn_cmake_add_arg(spn_cmake_t* cmake, const c8* arg);
void            spn_cmake_configure(spn_cmake_t* cmake);
void            spn_cmake_build(spn_cmake_t* cmake);
void            spn_cmake_install(spn_cmake_t* cmake);
void            spn_cmake_run(spn_cmake_t* cmake);

void            spn_copy(spn_build_ctx_t* build, spn_dir_kind_t from, const c8* from_path, spn_dir_kind_t to, const c8* to_path);

// @claude
void spn_pkg_set_name(spn_pkg_t* pkg, const c8* name);
void spn_pkg_set_repo(spn_pkg_t* pkg, const c8* repo);
void spn_pkg_set_author(spn_pkg_t* pkg, const c8* author);
void spn_pkg_set_maintainer(spn_pkg_t* pkg, const c8* maintainer);
void spn_pkg_add_version(spn_pkg_t* pkg, const c8* version, const c8* commit);
void spn_pkg_add_include(spn_pkg_t* pkg, const c8* path);
void spn_pkg_add_define(spn_pkg_t* pkg, const c8* define);
void spn_pkg_add_system_dep(spn_pkg_t* pkg, const c8* dep);
void spn_pkg_add_linkage(spn_pkg_t* pkg, spn_pkg_linkage_t linkage);

void            spn_log(spn_build_ctx_t* build, const c8* message);
spn_bin_ctx_t*  spn_get_target(spn_build_ctx_t* build, const c8* name);
spn_libc_kind_t spn_get_libc(spn_build_ctx_t* ctx);

#endif
