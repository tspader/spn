#ifndef SPN_CC_H
#define SPN_CC_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"
#include "compiler/types.h"
#include "target/types.h"
#include "toolchain/types.h"

#include "external/obj.h"

typedef struct {
  sp_str_t path;
  sp_str_t symbol;
  u64 size;
  spn_embed_types_t types;
} spn_cc_embed_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  spn_obj_builder_t obj;
  sp_da(spn_cc_embed_t) entries;
} spn_cc_embed_ctx_t;

sp_str_t         spn_cc_symbol_from_embedded_file(sp_mem_t mem, sp_str_t file_path);
void             spn_cc_embed_ctx_init(spn_cc_embed_ctx_t* ctx, sp_mem_t mem, spn_os_t target_os, spn_arch_t target_arch);
void             spn_cc_embed_ctx_free(spn_cc_embed_ctx_t* ctx);
spn_err_t        spn_cc_embed_ctx_add(spn_cc_embed_ctx_t* ctx, sp_mem_buffer_t data, sp_str_t symbol, sp_str_t path, sp_str_t data_type, sp_str_t size_type);
spn_err_t        spn_cc_embed_ctx_write(spn_cc_embed_ctx_t* ctx, sp_str_t object, sp_str_t header);

#endif
