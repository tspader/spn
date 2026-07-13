#ifndef SPN_GRAPH_GRAPH_H
#define SPN_GRAPH_GRAPH_H

#include "cc.h"
#include "unit/types.h"
#include "toolchain/sha256.h"
#include "sp/sp_graph.h"

typedef struct spn_dag_action_t spn_dag_action_t;
typedef u8 spn_dag_digest_t [32];

SP_TYPEDEF_FN(s32, spn_dag_exec_fn_t, spn_dag_action_t*, void*);
SP_TYPEDEF_FN(void, spn_dag_hash_fn_t, void*, u64, spn_dag_digest_t);

typedef u64 spn_dag_hash_t;

typedef struct {
  u32 index;
  u32 occupied;
} spn_dag_id_t;

typedef enum {
  SPN_DAG_ARTIFACT_KIND_VALUE,
  SPN_DAG_ARTIFACT_KIND_FILE,
} spn_dag_artifact_kind_t;

typedef struct {
  spn_dag_id_t id;
  spn_dag_artifact_kind_t kind;
  spn_dag_digest_t digest;
  spn_dag_id_t producer;
  sp_da(spn_dag_id_t) consumers;
} spn_dag_artifact_t;

struct spn_dag_action_t {
  spn_dag_id_t id;
  spn_bg_fn_t execute;
  spn_dag_hash_fn_t hash;
  void* user_data;
  sp_da(spn_dag_id_t) consumes;
  sp_da(spn_dag_id_t) produces;
};

typedef struct {
  spn_dag_exec_fn_t execute;
  spn_dag_hash_fn_t hash;
  void* user_data;
} spn_dag_action_config_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_da(spn_dag_artifact_t) artifacts;
  sp_da(spn_dag_action_t) actions;
} spn_dag_t;

spn_dag_t* spn_dag_new(sp_mem_t mem) {
  spn_dag_t* g = sp_alloc_type(mem, spn_dag_t);
  g->arena = sp_mem_arena_new(mem);
  g->mem = sp_mem_arena_as_allocator(g->arena);
  g->artifacts = sp_da_new(g->mem, spn_dag_artifact_t);
  g->actions = sp_da_new(g->mem, spn_dag_action_t);
  return g;
}

spn_dag_artifact_t* spn_dag_find_artifact(spn_dag_t* g, spn_dag_id_t id) {
  return g->artifacts + id.index;
}

spn_dag_action_t* spn_dag_find_action(spn_dag_t* g, spn_dag_id_t id) {
  return g->actions + id.index;
}

void spn_dag_action_add_input(spn_dag_t* g, spn_dag_id_t action, spn_dag_id_t artifact) {
  sp_assert(action.occupied);
  sp_assert(artifact.occupied);

  struct {
    spn_dag_action_t* action;
    spn_dag_artifact_t* artifact;
  } nodes = sp_zero;
  nodes.action = spn_dag_find_action(g, action);
  nodes.artifact = spn_dag_find_artifact(g, artifact);
  sp_da_push(nodes.action->consumes, artifact);
  sp_da_push(nodes.artifact->consumers, action);
}

void spn_dag_hash_scalar(void* ptr, u64 len, spn_dag_digest_t digest) {
  spn_sha256(ptr, len, digest);
}

s32 spn_dag_combine(spn_dag_action_t* action, void* user_data) {
  spn_dag_t* g = sp_ptr_cast(spn_dag_t*, user_data);

  spn_sha256_ctx_t c = sp_zero;
  spn_sha256_init(&c);
  sp_da_for(action->consumes, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->consumes[it]);
    spn_sha256_update(&c, artifact->digest, sizeof(artifact->digest));
  }

  // I don't know the exact API here
  spn_dag_digest_t digest = sp_zero;
  spn_sha256_final(&c, digest);
  return SPN_OK;
}

spn_dag_id_t spn_dag_add_action(spn_dag_t* g, spn_dag_action_config_t action) {
  return sp_zero_s(spn_dag_id_t);
}

spn_dag_id_t spn_dag_add_value(spn_dag_t* g, spn_dag_hash_t hash) {
  spn_dag_artifact_t artifact = {
    .id = {
      .index = (u32)sp_da_size(g->artifacts),
      .occupied = true
    },
  };
  sp_da_push(g->artifacts, artifact);
  return artifact.id;
}

spn_dag_id_t spn_dag_add_values(spn_dag_t* g, spn_dag_hash_t* hashes, u32 len) {
  spn_dag_id_t combine = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .hash = spn_dag_hash_scalar,
    .execute = spn_dag_combine,
    .user_data = g
  });
  sp_for(it, len) {
    spn_dag_id_t node = spn_dag_add_value(g, hashes[it]);
    spn_dag_action_add_input(g, combine, node);
  }
  return combine;
}


typedef struct {
  spn_dag_id_t toolchain;
  spn_dag_id_t os;
  spn_dag_id_t arch;
  spn_dag_id_t abi;
  spn_dag_id_t linkage;
  spn_dag_id_t standard;
  spn_dag_id_t mode;
  spn_dag_id_t opt;
  spn_dag_id_t sanitizers;
} spn_build_nodes_t;

void add_build(sp_mem_t mem, spn_build_unit_t* build) {
  spn_dag_t* g = spn_dag_new(mem);

  spn_build_nodes_t nodes = sp_zero;
  nodes.toolchain = spn_dag_add_value(g, build->toolchain->identity);
  nodes.os = spn_dag_add_value(g, build->profile.os);
  nodes.arch = spn_dag_add_value(g, build->profile.arch);
  nodes.abi = spn_dag_add_value(g, build->profile.abi);
  nodes.linkage = spn_dag_add_value(g, build->profile.linkage);
  nodes.standard = spn_dag_add_value(g, build->profile.standard);
  nodes.mode = spn_dag_add_value(g, build->profile.mode);
  nodes.opt = spn_dag_add_value(g, build->profile.opt);
  nodes.sanitizers = spn_dag_add_value(g, build->profile.sanitizers);
}

void add_target(sp_mem_t mem, spn_dag_t* g, spn_build_nodes_t* n) {

}

#endif
