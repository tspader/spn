#ifndef SPN_TEST_TOOLS_CTX_H
#define SPN_TEST_TOOLS_CTX_H

#include "sp.h"
#include "fixture.h"

typedef struct {
  tmpfs_t fs;
  sp_mem_arena_t* arena;
} ctx_t;

typedef struct {
  sp_str_t repo;
  struct {
    sp_str_t dir;
    sp_str_t fixtures;
  } test;
} ctx_paths_t;

ctx_t* ctx_get();
void   ctx_init(ctx_t* ctx);
void   ctx_deinit(ctx_t* ctx);
ctx_paths_t ctx_get_paths(ctx_t* ctx);

#endif
