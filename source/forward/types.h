#ifndef SPN_FORWARD_H
#define SPN_FORWARD_H

#ifndef SP_SP_H
  #include <stdint.h>

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

typedef struct spn_pkg spn_pkg_t;
typedef struct spn_target spn_target_t;
typedef struct spn_profile spn_profile_t;
typedef struct spn_index spn_index_t;
typedef struct spn_autoconf spn_autoconf_t;
typedef struct spn_make spn_make_t;
typedef struct spn_cmake spn_cmake_t;
typedef struct spn_cc spn_cc_t;
typedef struct spn_config spn_config_t;
typedef struct spn_build spn_build_t;
typedef struct spn_node_t spn_node_t;
typedef struct spn_pkg_unit_t spn_pkg_unit_t;
typedef struct spn_node_ctx_t spn_node_ctx_t;

typedef struct toml_table_t toml_table_t;

typedef s32  (*spn_node_fn_t)(spn_node_ctx_t*);

#endif
