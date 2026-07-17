#ifndef SPN_COMPILER_TOC_H
#define SPN_COMPILER_TOC_H

#include "sp.h"
#include "spn.h"

#include "error/types.h"

#define SPN_TOC_SYMBOL_MAX 8192

typedef enum {
  SPN_TOC_FORMAT_NONE,
  SPN_TOC_FORMAT_GNU,
  SPN_TOC_FORMAT_BSD,
} spn_toc_format_t;

typedef struct {
  sp_io_limit_reader_t body;
  spn_toc_format_t format;
  spn_err_t err;
  u64 count;
  u64 remaining;
  u8 buf [SPN_TOC_SYMBOL_MAX];
} spn_toc_parser_t;

spn_err_t spn_toc_init(spn_toc_parser_t* toc, sp_io_reader_t* io);
bool      spn_toc_next(spn_toc_parser_t* toc, sp_str_t* symbol);

#endif
