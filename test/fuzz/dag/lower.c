#include "fuzz.h"

#include "paths/paths.h"
#include "io/io.h"

typedef struct {
  fz_lowered_t* low;
  u64 action;
} fz_exec_ctx_t;

static spn_err_t fz_exec(spn_dag_t* g, spn_dag_action_t* action, void* user_data, spn_dag_env_t* env, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  fz_exec_ctx_t* ctx = (fz_exec_ctx_t*)user_data;
  fz_lowered_t* low = ctx->low;
  fz_action_t* fz = &low->u->actions[ctx->action];
  low->execs[ctx->action]++;
  fz_journal_exec(low->journal, ctx->action);
  if (low->ex) {
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
        if (sp_io_read_file(mem, spn_path_str(low->roots, mem, in->materialized), &inputs[it])) {
          return SPN_ERR_DAG_ACTION;
        }
        break;
      }
      case SPN_DAG_ARTIFACT_KIND_TREE: {
        sp_unreachable_return(SPN_ERR_DAG_ACTION);
      }
    }
  }

  sp_da_for(fz->obs, ot) {
    fz_obs_t fo = fz->obs[ot];
    sp_str_t path = fo.probe
      ? fz_phantom_sim_path(mem, fo.phantom)
      : fz_artifact_sim_path(mem, low->u, fo.artifact);
    sp_str_t bytes = sp_zero;
    sp_err_t err = sp_io_read_file(mem, path, &bytes);
    if (!err) {
      inputs[consumed + ot] = bytes;
      continue;
    }
    if (err != SP_ERR_SYS_NOT_FOUND) {
      return SPN_ERR_DAG_ACTION;
    }
    if (fo.probe) {
      if (low->state->phantoms[fo.phantom].present) {
        return SPN_ERR_DAG_ACTION;
      }
    }
    else if (spn_dag_digest_valid(spn_dag_find_artifact(low->g, low->ids[fo.artifact])->digest)) {
      return SPN_ERR_DAG_ACTION;
    }
    inputs[consumed + ot] = sp_str_lit("absent");
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* out = spn_dag_find_artifact(low->g, action->produces[it]);
    sp_str_t content = fz_output_content(mem, low->u->actions[ctx->action].identity, inputs, count, out->name);
    if (sp_fs_create_file_str(spn_path_str(low->roots, mem, out->materialized), content)) {
      return SPN_ERR_DAG_ACTION;
    }
  }

  sp_da_for(fz->obs, ot) {
    fz_obs_t fo = fz->obs[ot];
    if (fo.probe) {
      sp_str_t path = fz_phantom_sim_path(mem, fo.phantom);
      sp_sys_file_meta_t meta = sp_zero;
      sp_err_t err = sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &meta);
      if (err && err != SP_ERR_SYS_NOT_FOUND) {
        return SPN_ERR_DAG_ACTION;
      }
      sp_da_push(*obs, ((spn_dag_obs_t) {
        .kind = !err && meta.kind == SP_FS_KIND_FILE ? SPN_DAG_OBS_FILE : SPN_DAG_OBS_ABSENT,
        .path = spn_path_make(g->roots, path),
      }));
    }
    else {
      sp_da_push(*obs, ((spn_dag_obs_t) {
        .kind = SPN_DAG_OBS_FILE,
        .path = spn_path_make(g->roots, fz_artifact_sim_path(mem, low->u, fo.artifact)),
      }));
    }
  }
  return SPN_OK;
}

void fz_roots_init(spn_path_roots_t* roots) {
  roots->dirs[SPN_PATH_ROOT_PROJECT] = sp_str_lit("/out");
  roots->dirs[SPN_PATH_ROOT_STORE] = sp_str_lit("/src");
}

void fz_lower(fz_lowered_t* low, sp_mem_t mem, fz_universe_t* u, const spn_path_roots_t* roots) {
  low->u = u;
  low->mem = mem;
  low->roots = roots;
  low->g = spn_dag_new(mem, roots);
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
        id = spn_dag_add_file(low->g, spn_path_make(roots, fz_artifact_sim_path(mem, u, it)));
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
      .kind = action->discover ? SPN_DAG_ACTION_DISCOVERED : SPN_DAG_ACTION_STATIC,
      .identity = spn_dag_digest(identity.data, identity.len),
      .execute = fz_exec,
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
