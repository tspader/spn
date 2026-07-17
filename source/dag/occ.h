#ifndef SPN_DAG_OCC_H
#define SPN_DAG_OCC_H

#include "sp.h"

typedef enum {
  OCC_OK,
  OCC_ERR_MISSING_COLON,
  OCC_ERR_BAD_CONTINUATION,
  OCC_ERR_UNTERMINATED_QUOTE,
  OCC_ERR_PREREQ_TOO_LONG,
} occ_err_t;

typedef struct {
  sp_str_t content;
  u64 n;
  occ_err_t err;
  c8 buf [SP_PATH_MAX];
} occ_parser_t;

occ_err_t occ_init(occ_parser_t* p, sp_str_t content);
bool occ_next(occ_parser_t* p, sp_str_t* prereq);

#endif
