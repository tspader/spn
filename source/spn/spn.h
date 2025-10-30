#ifndef SPN_SPN_H
#define SPN_SPN_H

#define _SP_MSTR(x) #x
#define SP_MSTR(x) _SP_MSTR(x)

#define _SP_MCAT(x, y) x##y
#define SP_MCAT(x, y) _SP_MCAT(x, y)

#define SPN_COPY_N_MAX_ENTRIES 16

#define SPN_DEP(expr) you_forgot_to_escape_a_dep_with_backslash spn_static_assert;

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
  SPN_DEP_BUILD_MODE_DEBUG,
  SPN_DEP_BUILD_MODE_RELEASE,
} spn_dep_mode_t;

typedef enum {
  SPN_DEP_BUILD_KIND_NONE,
  SPN_DEP_BUILD_KIND_SHARED,
  SPN_DEP_BUILD_KIND_STATIC,
  SPN_DEP_BUILD_KIND_SOURCE,
} spn_dep_kind_t;

typedef enum {
  SPN_DIR_NONE,
  SPN_DIR_CACHE,
  SPN_DIR_STORE,
  SPN_DIR_INCLUDE,
  SPN_DIR_VENDOR,
  SPN_DIR_LIB,
  SPN_DIR_SOURCE,
  SPN_DIR_WORK,
} spn_cache_dir_kind_t;

///////////
// BUILD //
///////////
#define SPN_MAX_DEPS 16

typedef struct {
  void* data;
  u32 size;
} spn_opaque_options_t;

typedef struct {
  const c8* name;
  const c8* lock;
  spn_dep_kind_t kind;
  spn_opaque_options_t options;
} spn_opaque_dep_t;

typedef struct {
  spn_opaque_dep_t* deps;
  u32 num_deps;
} spn_opaque_build_t;

////////////
// RECIPE //
////////////
#define SPN_RECIPE_MAX_LIBS 8

typedef struct {
  const c8* name;
  spn_dep_kind_t kind;
  spn_dep_mode_t mode;
  void* options;

  const c8* commit;
  const c8* build_id;

  struct {
    const c8* log;
    const c8* stamp;
    const c8* source;
    const c8* work;
    const c8* store;
    const c8* include;
    const c8* lib;
    const c8* vendor;
  } paths;
} spn_dep_builder_t;

typedef struct {
  spn_cache_dir_kind_t dir;
  const c8* path;
} spn_recipe_path_t;

typedef struct {
  spn_recipe_path_t from;
  spn_recipe_path_t to;
} spn_recipe_copy_config_t;

typedef void (*spn_recipe_build_fn_t)(spn_dep_builder_t);
typedef void (*spn_recipe_configure_fn_t)(spn_dep_builder_t);
typedef void (*spn_recipe_package_fn_t)(spn_dep_builder_t);

typedef struct spn_recipe_config {
  const c8* name;
  const c8* git;
  const c8* branch;
  const c8* libs [SPN_RECIPE_MAX_LIBS];
  spn_dep_kind_t kinds [3];
  spn_recipe_configure_fn_t configure;
  spn_recipe_build_fn_t build;
  spn_recipe_package_fn_t package;
} spn_recipe_info_t;

typedef spn_recipe_info_t (*spn_recipe_fn_t)(void);

#define SPN_SINGLE_HEADER(DEP, GIT, HEADER) \
  void SP_MCAT(DEP, _package)(spn_dep_builder_t build) { \
    spn_copy(&build, SPN_DIR_SOURCE, HEADER, SPN_DIR_INCLUDE, NULL); \
  } \
 \
  spn_recipe_info_t DEP() { \
    return (spn_recipe_info_t) { \
      .name = SP_MSTR(DEP), \
      .git = GIT, \
      .kinds = { \
        SPN_DEP_BUILD_KIND_SOURCE \
      }, \
      .package = SP_MCAT(DEP, _package), \
    }; \
  }

typedef struct {
  const c8* target;
  u32 jobs;
} spn_make_t;

typedef struct {
  int foo;
} spn_autoconf_t;

void spn_make(spn_dep_builder_t* build, spn_make_t config);
void spn_autoconf(spn_dep_builder_t* build, spn_autoconf_t config);

void spn_copy(spn_dep_builder_t* build, spn_cache_dir_kind_t from, const c8* from_path, spn_cache_dir_kind_t to, const c8* to_path);
void spn_copy_n(spn_dep_builder_t* build, spn_recipe_copy_config_t entries [SPN_COPY_N_MAX_ENTRIES]);

////////////
// CONFIG //
////////////
typedef struct {
  const char* spn;
} spn_user_config_t;

#endif
