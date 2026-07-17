#include "fuzz.h"
#include "sha256/sha256.h"

sp_str_t fz_content(sp_mem_t mem, u64 content) {
  return sp_fmt(mem, "c{}", sp_fmt_uint(content)).value;
}

sp_str_t fz_output_name(sp_mem_t mem, u64 artifact) {
  return sp_fmt(mem, "f{}", sp_fmt_uint(artifact)).value;
}

static void fz_hash_u64(spn_sha256_ctx_t* ctx, u64 value) {
  spn_sha256_update(ctx, (const u8*)&value, sizeof(value));
}

static void fz_hash_str(spn_sha256_ctx_t* ctx, sp_str_t str) {
  fz_hash_u64(ctx, str.len);
  spn_sha256_update(ctx, (const u8*)str.data, str.len);
}

sp_str_t fz_output_content(sp_mem_t mem, u64 identity, const sp_str_t* inputs, u64 count, sp_str_t name) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  fz_hash_str(&ctx, sp_str_lit("fz.output.v1"));
  fz_hash_u64(&ctx, identity);
  fz_hash_u64(&ctx, count);
  sp_for(it, count) {
    fz_hash_str(&ctx, inputs[it]);
  }
  fz_hash_str(&ctx, name);

  spn_dag_digest_t digest = sp_zero;
  spn_sha256_final(&ctx, digest.bytes);
  return spn_dag_digest_hex(mem, digest);
}

u64 fz_action_inputs(sp_mem_t mem, fz_universe_t* u, u64 at, const sp_str_t* bytes, sp_str_t** inputs) {
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
      fz_phantom_t* phantom = &u->phantoms[obs.phantom];
      (*inputs)[consumed + ot] = phantom->present ? fz_content(mem, phantom->content) : sp_str_lit("absent");
    }
    else {
      (*inputs)[consumed + ot] = bytes[obs.artifact];
    }
  }
  return count;
}

static void fz_expect_action(sp_mem_t mem, fz_universe_t* u, sp_str_t* bytes, bool* done, u64 at) {
  if (done[at]) {
    return;
  }
  done[at] = true;

  fz_action_t* action = &u->actions[at];
  sp_da_for(action->consumes, ct) {
    s64 producer = u->artifacts[action->consumes[ct]].producer;
    if (producer >= 0) {
      fz_expect_action(mem, u, bytes, done, (u64)producer);
    }
  }
  sp_da_for(action->obs, ot) {
    fz_obs_t obs = action->obs[ot];
    if (obs.probe) {
      continue;
    }
    s64 producer = u->artifacts[obs.artifact].producer;
    if (producer >= 0) {
      fz_expect_action(mem, u, bytes, done, (u64)producer);
    }
  }

  sp_str_t* inputs = SP_NULLPTR;
  u64 count = fz_action_inputs(mem, u, at, bytes, &inputs);
  sp_da_for(action->produces, pt) {
    u64 out = action->produces[pt];
    bytes[out] = fz_output_content(mem, action->identity, inputs, count, fz_output_name(mem, out));
  }
}

void fz_expect(sp_mem_t mem, fz_universe_t* u, sp_str_t* bytes) {
  sp_assert(!u->cyclic && !u->obs_cyclic);
  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
      bytes[it] = fz_content(mem, u->artifacts[it].content);
    }
  }

  u64 actions = sp_da_size(u->actions);
  bool* done = sp_alloc_n(mem, bool, actions ? actions : 1);
  sp_mem_zero(done, actions * sizeof(bool));
  sp_da_for(u->actions, at) {
    fz_expect_action(mem, u, bytes, done, at);
  }
}

spn_dag_digest_t fz_model_key(fz_universe_t* u, const sp_str_t* bytes, u64 at) {
  fz_action_t* action = &u->actions[at];

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  fz_hash_str(&ctx, sp_str_lit("fz.key.v1"));
  fz_hash_u64(&ctx, at);
  fz_hash_u64(&ctx, action->identity);
  fz_hash_u64(&ctx, sp_da_size(action->consumes));
  sp_da_for(action->consumes, ct) {
    fz_hash_str(&ctx, bytes[action->consumes[ct]]);
  }

  spn_dag_digest_t key = sp_zero;
  spn_sha256_final(&ctx, key.bytes);
  return key;
}

fz_shape_t fz_shape_now(sp_mem_t mem, fz_universe_t* u, u64 at) {
  u64 count = sp_da_size(u->actions[at].obs);
  fz_shape_t shape = { .file = sp_alloc_n(mem, bool, count ? count : 1) };
  sp_mem_zero(shape.file, count * sizeof(bool));
  sp_da_for(u->actions[at].obs, ot) {
    fz_obs_t obs = u->actions[at].obs[ot];
    shape.file[ot] = !obs.probe || u->phantoms[obs.phantom].present;
  }
  return shape;
}

spn_dag_digest_t fz_model_strong(fz_universe_t* u, const sp_str_t* bytes, spn_dag_digest_t prelim, u64 at, const fz_shape_t* shape) {
  fz_action_t* action = &u->actions[at];

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  fz_hash_str(&ctx, sp_str_lit("fz.strong.v1"));
  spn_sha256_update(&ctx, prelim.bytes, sizeof(prelim.bytes));
  fz_hash_u64(&ctx, sp_da_size(action->obs));
  sp_da_for(action->obs, ot) {
    fz_obs_t obs = action->obs[ot];
    fz_hash_u64(&ctx, shape->file[ot]);
    if (obs.probe) {
      fz_phantom_t* phantom = &u->phantoms[obs.phantom];
      fz_hash_u64(&ctx, sp_da_size(u->artifacts) + obs.phantom);
      fz_hash_u64(&ctx, phantom->present ? phantom->content : (u64)-1);
    }
    else {
      fz_hash_u64(&ctx, obs.artifact);
      fz_hash_str(&ctx, bytes[obs.artifact]);
    }
  }

  spn_dag_digest_t key = sp_zero;
  spn_sha256_final(&ctx, key.bytes);
  return key;
}
