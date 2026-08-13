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

#endif
