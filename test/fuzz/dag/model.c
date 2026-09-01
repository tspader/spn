#include "fuzz.h"
#include "hash/sha256/sha256.h"
#include "paths/paths.h"

sp_str_t fz_content(sp_mem_t mem, u64 content) {
  return sp_fmt(mem, "c{}", sp_fmt_uint(content)).value;
}

sp_str_t fz_output_name(sp_mem_t mem, u64 artifact) {
  return sp_fmt(mem, "f{}", sp_fmt_uint(artifact)).value;
}

static void hash_u64(spn_sha256_ctx_t* ctx, u64 value) {
  spn_sha256_update(ctx, (const u8*)&value, sizeof(value));
}

static void hash_str(spn_sha256_ctx_t* ctx, sp_str_t str) {
  hash_u64(ctx, str.len);
  spn_sha256_update(ctx, (const u8*)str.data, str.len);
}

sp_str_t fz_output_content(sp_mem_t mem, u64 identity, const sp_str_t* inputs, u64 count, sp_str_t name) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  hash_str(&ctx, sp_str_lit("fz.output.v1"));
  hash_u64(&ctx, identity);
  hash_u64(&ctx, count);
  sp_for(it, count) {
    hash_str(&ctx, inputs[it]);
  }
  hash_str(&ctx, name);

  spn_dag_digest_t digest = sp_zero;
  spn_sha256_final(&ctx, digest.bytes);
  return spn_dag_digest_hex(mem, digest);
}

u64 fz_action_inputs(sp_mem_t mem, fz_universe_t* u, const fz_state_t* state, u64 at, const sp_str_t* bytes, sp_str_t** inputs) {
  fz_action_t* action = &u->actions[at];
  u64 consumed = sp_da_size(action->consumes);
  u64 count = consumed + sp_da_size(action->obs);
  *inputs = sp_alloc_n(mem, sp_str_t, count ? count : 1);
  sp_da_for(action->consumes, ct) {
    (*inputs)[ct] = bytes[action->consumes[ct]];
  }
  sp_da_for(action->obs, ot) {
    fz_obs_t obs = action->obs[ot];
    if (obs.probe) {
      fz_phantom_t* phantom = &state->phantoms[obs.phantom];
      (*inputs)[consumed + ot] = phantom->present ? fz_content(mem, phantom->content) : sp_str_lit("absent");
    }
    else {
      (*inputs)[consumed + ot] = bytes[obs.artifact];
    }
  }
  return count;
}

fz_shape_t fz_shape_now(sp_mem_t mem, fz_universe_t* u, const fz_state_t* state, u64 at) {
  u64 count = sp_da_size(u->actions[at].obs);
  fz_shape_t shape = { .file = sp_alloc_n(mem, bool, count ? count : 1) };
  sp_mem_zero(shape.file, count * sizeof(bool));
  sp_da_for(u->actions[at].obs, ot) {
    fz_obs_t obs = u->actions[at].obs[ot];
    shape.file[ot] = !obs.probe || state->phantoms[obs.phantom].present;
  }
  return shape;
}

spn_dag_digest_t fz_model_weak(sp_mem_t mem, fz_universe_t* u, const sp_str_t* bytes, u64 at) {
  fz_action_t* action = &u->actions[at];

  sp_str_t identity = sp_fmt(mem, "id{}", sp_fmt_uint(action->identity)).value;
  spn_digest_ctx_t ctx = sp_zero;
  spn_digest_init_blake3(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.dag.action.v4"));
  spn_dag_hash_u64(&ctx, 0);
  spn_dag_hash_digest(&ctx, spn_dag_digest(identity.data, identity.len));

  spn_dag_hash_u64(&ctx, sp_da_size(action->consumes));
  sp_da_for(action->consumes, ct) {
    u64 in = action->consumes[ct];
    spn_dag_artifact_kind_t kind = u->artifacts[in].kind == FZ_ARTIFACT_VALUE
      ? SPN_DAG_ARTIFACT_KIND_VALUE
      : SPN_DAG_ARTIFACT_KIND_FILE;
    spn_dag_hash_u8(&ctx, (u8)kind);
    spn_dag_hash_digest(&ctx, spn_dag_digest(bytes[in].data, bytes[in].len));
  }

  spn_dag_hash_u64(&ctx, sp_da_size(action->produces));
  sp_da_for(action->produces, pt) {
    spn_dag_hash_u8(&ctx, (u8)SPN_DAG_ARTIFACT_KIND_FILE);
    spn_dag_hash_str(&ctx, fz_output_name(mem, action->produces[pt]));
  }

  return spn_dag_hash_final(&ctx);
}

u32 fz_model_obs(sp_mem_t mem, fz_universe_t* u, const fz_state_t* state, const sp_str_t* bytes, u64 at, const fz_shape_t* shape, spn_dag_obs_t** obs) {
  spn_path_roots_t roots = sp_zero;
  fz_roots_init(&roots);

  sp_da(spn_dag_obs_t) rows = sp_da_new(mem, spn_dag_obs_t);
  sp_da_for(u->actions[at].obs, ot) {
    fz_obs_t fo = u->actions[at].obs[ot];
    sp_str_t path = fo.probe
      ? fz_phantom_sim_path(mem, fo.phantom)
      : fz_artifact_sim_path(mem, u, fo.artifact);

    spn_dag_digest_t digest = sp_zero;
    if (fo.probe) {
      if (state->phantoms[fo.phantom].present) {
        sp_str_t content = fz_content(mem, state->phantoms[fo.phantom].content);
        digest = spn_dag_digest(content.data, content.len);
      }
    }
    else {
      digest = spn_dag_digest(bytes[fo.artifact].data, bytes[fo.artifact].len);
    }

    sp_da_push(rows, ((spn_dag_obs_t) {
      .kind = shape->file[ot] ? SPN_DAG_OBS_FILE : SPN_DAG_OBS_ABSENT,
      .path = spn_path_make(&roots, path),
      .meta = { .digest = digest },
    }));
  }

  spn_dag_obs_canonicalize(rows);
  *obs = rows;
  return (u32)sp_da_size(rows);
}
