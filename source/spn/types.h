#ifndef SPN_TYPES_H
#define SPN_TYPES_H

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
#endif
