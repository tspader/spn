#ifndef SPN_EMBED_H
#define SPN_EMBED_H

#include "sp.h"

#define SP_EMBED_DEFAULT_SYMBOL_S sp_str_lit("")
#define SP_EMBED_DEFAULT_DATA_T_S sp_str_lit("")
#define SP_EMBED_DEFAULT_SIZE_T_S sp_str_lit("")

typedef enum {
  SPN_EMBED_FILE,
  SPN_EMBED_MEM,
} spn_embed_kind_t;

typedef struct {
  sp_str_t data;
  sp_str_t size;
} spn_embed_types_t;

typedef struct {
  spn_embed_kind_t kind;
  sp_str_t symbol;
  spn_embed_types_t types;
  union {
    struct { sp_str_t path; } file;
    struct { const u8* buffer; u64 size; } memory;
  };
} spn_embed_t;

#endif
