#ifndef SPN_LOG_LAZY_TYPES_H
#define SPN_LOG_LAZY_TYPES_H

#include "sp.h"

typedef struct {
  sp_io_writer_t      writer;
  sp_io_file_writer_t file;
  sp_str_t            path;
  bool                opened;
  bool                failed;
} spn_lazy_log_t;

typedef struct spn_build_io_t {
  spn_lazy_log_t build;
  spn_lazy_log_t test;
  spn_lazy_log_t jsonl;
} spn_build_io_t;

#endif
