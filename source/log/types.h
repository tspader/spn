#ifndef SPN_LOG_TYPES_H
#define SPN_LOG_TYPES_H

#include "sp.h"

typedef enum {
  SPN_LOG_LEVEL_ERROR,
  SPN_LOG_LEVEL_WARN,
  SPN_LOG_LEVEL_INFO,
  SPN_LOG_LEVEL_DEBUG,
} spn_log_level_t;

typedef enum {
  SPN_VERBOSITY_QUIET,
  SPN_VERBOSITY_NORMAL,
  SPN_VERBOSITY_VERBOSE,
  SPN_VERBOSITY_DEBUG,
} spn_verbosity_t;

typedef struct {
  sp_io_stream_writer_t out;
  sp_io_stream_writer_t err;
  sp_io_file_writer_t jsonl;
  spn_log_level_t level;
  spn_verbosity_t verbosity;
} spn_logger_t;

#endif
