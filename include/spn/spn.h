#ifndef SPN_SPN_H
#define SPN_SPN_H

#ifndef SP_SP_H
  #include <stdint.h>
  #include <stdbool.h>
  #include <stdlib.h>
  #include <string.h>

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
} spn_dep_mode_t;

typedef enum {
  SPN_LIB_KIND_NONE = 0,
  SPN_LIB_KIND_SHARED = 1,
  SPN_LIB_KIND_STATIC = 2,
  SPN_LIB_KIND_SOURCE = 3,
} spn_lib_kind_t;

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

typedef struct spn_config spn_config_t;
typedef struct spn_package spn_package_t;
typedef struct spn_dep_context spn_dep_context_t;
typedef struct spn_autoconf spn_autoconf_t;
typedef struct spn_make spn_make_t;

typedef void(*spn_config_fn_t)(spn_config_t*);
typedef void(*spn_dep_fn_t)(spn_dep_context_t*);

void            spn_make(spn_dep_context_t* build);
spn_make_t*     spn_make_new(spn_dep_context_t* build);
void            spn_make_add_target(spn_make_t* make, const c8* target);
void            spn_make_run(spn_make_t* make);
void            spn_autoconf(spn_dep_context_t* build);
spn_autoconf_t* spn_autoconf_new(spn_dep_context_t* build);
void            spn_autoconf_run(spn_autoconf_t* autoconf);
void            spn_dep_log(spn_dep_context_t* dep, const c8* message);
void            spn_dep_set_u32(spn_dep_context_t* dep, const c8* name, u32 value);
void            spn_copy(spn_dep_context_t* build, spn_dir_kind_t from, const c8* from_path, spn_dir_kind_t to, const c8* to_path);

#endif
