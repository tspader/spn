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
  low->execs[ctx->action]++;
  if (low->ex) {
    low->ex->ran = (s64)ctx->action;
  }

  u64 count = sp_da_size(action->consumes);
  sp_str_t* inputs = sp_alloc_n(mem, sp_str_t, count);
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

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* out = spn_dag_find_artifact(low->g, action->produces[it]);
    sp_str_t content = fz_output_content(mem, low->u->actions[ctx->action].identity, inputs, count, out->name);
    if (sp_fs_create_file_str(out->path, content)) {
      return 1;
    }
  }
  return 0;
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
