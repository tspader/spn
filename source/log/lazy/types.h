#ifndef SPN_LOG_LAZY_TYPES_H
#define SPN_LOG_LAZY_TYPES_H

#include "sp.h"

typedef struct {
  sp_io_writer_t     writer;
  sp_str_t           path;
  sp_io_write_mode_t mode;
  bool               opened;
} spn_lazy_log_t;

#endif
