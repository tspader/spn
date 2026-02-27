#ifndef SPN_ERR_H
#define SPN_ERR_H

#include "sp.h"

#ifndef SPN_ERR_T_DEFINED
#define SPN_ERR_T_DEFINED
typedef enum {
  SPN_OK,
  SPN_ERROR,
} spn_err_t;

typedef enum {
  SPN_ERR_KIND_NONE,
  SPN_ERR_KIND_MANIFEST_PARSE,
  SPN_ERR_KIND_MANIFEST_FIELD,
} spn_err_kind_t;

typedef struct {
  spn_err_t code;
  spn_err_kind_t kind;
  union {
    struct {
      sp_str_t path;
    } manifest_parse;
    struct {
      sp_str_t path;
      sp_str_t expected;
      sp_str_t actual;
    } manifest_field;
  };
} spn_err_union_t;
#endif

#endif
