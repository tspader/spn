#include "fuzz.h"
#include "sp/io.h"

typedef struct {
  fz_lowered_t* low;
  u64 action;
} fz_exec_ctx_t;

static s32 fz_exec(spn_dag_action_t* action, void* user_data) {
  fz_exec_ctx_t* ctx = (fz_exec_ctx_t*)user_data;
  fz_lowered_t* low = ctx->low;
  sp_mem_t mem = low->mem;
  fz_action_t* fz = &low->u->actions[ctx->action];
  low->execs[ctx->action]++;
  fz_journal_exec(low->journal, ctx->action);
  if (low->ex) {
    low->ex->ran = (s64)ctx->action;
    sp_da_push(low->ex->log, ((fz_flight_t) {
      .action = ctx->action,
      .started = low->ex->sim->syscalls,
    }));
  }

  u64 consumed = sp_da_size(action->consumes);
  u64 count = consumed + sp_da_size(fz->obs);
  sp_str_t* inputs = sp_alloc_n(mem, sp_str_t, count ? count : 1);
  sp_da_for(action->consumes, it) {
    spn_dag_artifact_t* in = spn_dag_find_artifact(low->g, action->consumes[it]);
    switch (in->kind) {
      case SPN_DAG_ARTIFACT_KIND_VALUE: {
        inputs[it] = fz_content(mem, low->u->artifacts[in->id.index].content);
        break;
      }
      case SPN_DAG_ARTIFACT_KIND_FILE: {
        if (sp_io_read_file(mem, in->path, &inputs[it])) {
          return 1;
        }
        break;
      }
      case SPN_DAG_ARTIFACT_KIND_TREE: {
        sp_unreachable_return(1);
      }
    }
  }

  sp_da_for(fz->obs, ot) {
    fz_obs_t obs = fz->obs[ot];
    sp_str_t path = obs.probe
      ? fz_phantom_sim_path(mem, obs.phantom)
      : fz_artifact_sim_path(mem, low->u, obs.artifact);
    sp_str_t bytes = sp_zero;
    if (!sp_io_read_file(mem, path, &bytes)) {
      inputs[consumed + ot] = bytes;
      continue;
    }
    if (obs.probe) {
      if (low->u->phantoms[obs.phantom].present) {
        return 1;
      }
    }
    else if (spn_dag_digest_valid(spn_dag_find_artifact(low->g, low->ids[obs.artifact])->digest)) {
      return 1;
    }
    inputs[consumed + ot] = sp_str_lit("absent");
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* out = spn_dag_find_artifact(low->g, action->produces[it]);
    sp_str_t content = fz_output_content(mem, low->u->actions[ctx->action].identity, inputs, count, out->name);
    if (sp_fs_create_file_str(out->path, content)) {
      return 1;
    }
  }
  return 0;
}

static spn_err_t fz_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  fz_exec_ctx_t* ctx = (fz_exec_ctx_t*)user_data;
  fz_lowered_t* low = ctx->low;

  sp_da_for(low->u->actions[ctx->action].obs, ot) {
    fz_obs_t obs = low->u->actions[ctx->action].obs[ot];
    if (obs.probe) {
      sp_str_t path = fz_phantom_sim_path(mem, obs.phantom);
      sp_da_push(*out, ((spn_dag_obs_t) {
        .kind = sp_fs_is_file(path) ? SPN_DAG_OBS_FILE : SPN_DAG_OBS_ABSENT,
        .path = path,
      }));
    }
    else {
      sp_da_push(*out, ((spn_dag_obs_t) {
        .kind = SPN_DAG_OBS_FILE,
        .path = fz_artifact_sim_path(mem, low->u, obs.artifact),
      }));
    }
  }
  return SPN_OK;
}

void fz_lower(fz_lowered_t* low, sp_mem_t mem, fz_universe_t* u) {
  low->u = u;
  low->mem = mem;
  low->g = spn_dag_new(mem);
  sp_da_init(mem, low->ids);
  sp_da_init(mem, low->execs);

  sp_da_for(u->artifacts, it) {
    fz_artifact_t* artifact = &u->artifacts[it];
    spn_dag_id_t id = sp_zero;
    switch (artifact->kind) {
      case FZ_ARTIFACT_VALUE: {
        sp_str_t content = fz_content(mem, artifact->content);
        id = spn_dag_add_value(low->g, content.data, content.len);
        break;
      }
      case FZ_ARTIFACT_SOURCE:
      case FZ_ARTIFACT_OUTPUT: {
        id = spn_dag_add_file(low->g, fz_artifact_sim_path(mem, u, it));
        break;
      }
    }
    sp_assert(id.index == it);
    sp_da_push(low->ids, id);
  }

  sp_da_for(u->actions, at) {
    fz_action_t* action = &u->actions[at];
    fz_exec_ctx_t* ctx = sp_alloc_type(mem, fz_exec_ctx_t);
    *ctx = (fz_exec_ctx_t) {
      .low = low,
      .action = at,
    };

    sp_str_t identity = sp_fmt(mem, "id{}", sp_fmt_uint(action->identity)).value;
    spn_dag_id_t id = spn_dag_add_action(low->g, (spn_dag_action_config_t) {
      .identity = spn_dag_digest(identity.data, identity.len),
      .execute = fz_exec,
      .discover = action->discover ? fz_discover : SP_NULLPTR,
      .user_data = ctx,
    });
    sp_assert(id.index == at);

    sp_da_for(action->consumes, ct) {
      spn_dag_action_add_input(low->g, id, low->ids[action->consumes[ct]]);
    }
    sp_da_for(action->produces, pt) {
      spn_err_t err = spn_dag_action_add_output(low->g, id, low->ids[action->produces[pt]]);
      sp_assert(!err);
    }
    sp_da_push(low->execs, 0);
  }
}
