#ifndef SPN_ERR_H
#define SPN_ERR_H

#include "sp.h"
#include "errors.gen.h"

#define spn_try(expr) \
  do { \
    spn_err_t __err = (expr); \
    if (__err) { \
      return __err; \
    } \
  } while (0)

#define spn_try_union(expr) \
  do { \
    spn_err_union_t __err = (expr); \
    if (__err.kind) { \
      return __err; \
    } \
  } while (0)

#define spn_result(kind_) ((spn_err_union_t) { .kind = (kind_) })

#define spn_err_reported(kind_) ((spn_err_union_t) { .kind = (kind_), .reported = true })

#endif
