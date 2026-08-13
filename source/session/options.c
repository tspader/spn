#include "sp.h"
#include "ctx/types.h"
#include "intern/types.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/types.h"

#include "error/error.h"
#include "error/types.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/options.h"
#include "session/session.h"
#include "when/when.h"

#define SPN_GATE_MAX_RESOLVES 4

static spn_err_t violation_kind(spn_option_err_t kind) {
  switch (kind) {
    case SPN_OPTION_ERR_UNDECLARED:  return SPN_ERR_OPTION_UNDECLARED;
    case SPN_OPTION_ERR_BAD_VALUE:   return SPN_ERR_OPTION_BAD_VALUE;
    case SPN_OPTION_ERR_CONFLICT:    return SPN_ERR_OPTION_CONFLICT;
    case SPN_OPTION_ERR_VETO:        return SPN_ERR_OPTION_VETO;
    case SPN_OPTION_ERR_NO_VALUE:    return SPN_ERR_OPTION_NO_VALUE;
    case SPN_OPTION_ERR_LATE_GATE:   return SPN_ERR_OPTION_LATE_GATE;
    case SPN_OPTION_ERR_UNKNOWN_PKG: return SPN_ERR_OPTION_UNKNOWN_PKG;
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_setter_t violation_setter(spn_option_setter_t setter) {
  return (spn_err_setter_t) { .kind = setter.kind, .name = setter.name };
}

static spn_err_union_t violation_err(sp_mem_t mem, spn_option_violation_t violation) {
  return (spn_err_union_t) {
    .kind = violation_kind(violation.kind),
    .option = {
      .pkg = violation.pkg,
      .option = violation.option,
      .value = violation.value.kind == SPN_OPTION_VALUE_NONE ? sp_str_lit("") : spn_option_value_to_str(mem, violation.value),
      .a = violation_setter(violation.a),
      .b = violation_setter(violation.b),
    },
  };
}

static spn_resolved_dep_t* node_find_edge(spn_resolved_pkg_t* node, sp_intern_id_t qualified, spn_dep_kind_t kind) {
  sp_da_for(node->edges, it) {
    if (node->edges[it].id.qualified == qualified && node->edges[it].kind == kind) {
      return &node->edges[it];
    }
  }
  return SP_NULLPTR;
}

static void node_prune_edge(spn_resolved_pkg_t* node, spn_resolved_dep_t* edge) {
  u64 index = (u64)(edge - node->edges);
  sp_for(it, sp_da_size(node->edges) - index - 1) {
    node->edges[index + it] = node->edges[index + it + 1];
  }
  sp_da_pop(node->edges);
}

static void sweep_unreachable(spn_session_t* session) {
  sp_mem_t mem = session->mem;

  sp_ht(spn_pkg_id_t, u8) visited = SP_NULLPTR;
  sp_ht_init(mem, visited);
  sp_da(spn_pkg_id_t) frontier = sp_da_new(mem, spn_pkg_id_t);

  sp_ht_for_kv(session->resolve, it) {
    if (it.val->source == SPN_PKG_SOURCE_ROOT) {
      sp_ht_insert(visited, it.val->id, (u8)true);
      sp_da_push(frontier, it.val->id);
    }
  }

  while (!sp_da_empty(frontier)) {
    spn_pkg_id_t id = *sp_da_back(frontier);
    sp_da_pop(frontier);

    spn_resolved_pkg_t* node = sp_ht_getp(session->resolve, id);
    if (!node) {
      continue;
    }
    sp_da_for(node->edges, it) {
      if (!sp_ht_getp(visited, node->edges[it].id)) {
        sp_ht_insert(visited, node->edges[it].id, (u8)true);
        sp_da_push(frontier, node->edges[it].id);
      }
    }
  }

  spn_resolve_t kept = SP_NULLPTR;
  sp_ht_init(mem, kept);
  sp_ht_for_kv(session->resolve, it) {
    if (sp_ht_getp(visited, it.val->id)) {
      sp_ht_insert(kept, *it.key, *it.val);
    }
  }
  session->resolve = kept;
}

static spn_err_t validate_config_keys(spn_session_t* session) {
  sp_da_for(session->pkg->config, it) {
    spn_pkg_config_entry_t* entry = &session->pkg->config[it];

    bool known = false;
    sp_ht_for_kv(session->resolve, rt) {
      spn_resolved_pkg_t* node = rt.val;
      spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
      sp_assert(loaded);
      if (sp_str_equal(node->name, entry->key)) {
        known = true;
        break;
      }
      sp_da_for(loaded->info->deps, dt) {
        if (sp_str_equal(spn_pkg_name_from_qualified(loaded->info->deps[dt].qualified).name, entry->key)) {
          known = true;
          break;
        }
      }
      if (known) {
        break;
      }
    }

    if (!known) {
      return spn_err_emit(session->ctx, (spn_err_union_t) {
        .kind = SPN_ERR_OPTION_UNKNOWN_PKG,
        .option = { .pkg = entry->key },
      });
    }
  }
  return SPN_OK;
}

spn_err_t spn_session_apply_options(spn_session_t* session, bool* reresolve) {
  sp_mem_t mem = session->mem;
  sp_str_t missing_pkg = sp_zero;
  sp_str_t missing_dep = sp_zero;
  *reresolve = false;

  for (;;) {
    sp_ht_init(mem, session->options);
    sp_ht_init(mem, session->gates.seeds);

    sp_ht(spn_pkg_id_t, spn_option_requests_t) requests = SP_NULLPTR;
    sp_ht_init(mem, requests);

    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
      sp_assert(loaded);

      sp_da_for(loaded->info->deps, dt) {
        spn_requested_dep_t* dep = &loaded->info->deps[dt];
        if (sp_da_empty(dep->options.clauses)) {
          continue;
        }
        spn_resolved_dep_t* edge = node_find_edge(node, sp_intern_get_or_insert(session->ctx->intern, dep->qualified), dep->kind);
        if (!edge) {
          continue;
        }

        spn_option_request_t request = {
          .consumer = node->name,
          .options = &dep->options,
        };

        if (!sp_ht_getp(requests, edge->id)) {
          sp_ht_insert(requests, edge->id, sp_da_new(mem, spn_option_request_t));
        }
        sp_da_push(*sp_ht_getp(requests, edge->id), request);

        spn_resolved_pkg_t* target = sp_ht_getp(session->resolve, edge->id);
        sp_intern_id_t seed_key = target ? target->id.qualified : sp_intern_get_or_insert(session->ctx->intern, dep->qualified);
        if (!sp_ht_getp(session->gates.seeds, seed_key)) {
          sp_ht_insert(session->gates.seeds, seed_key, sp_da_new(mem, spn_option_request_t));
        }
        sp_da_push(*sp_ht_getp(session->gates.seeds, seed_key), request);
      }
    }

    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_option_requests_t* asked = sp_ht_getp(requests, node->id);

      spn_option_requests_t node_requests = asked ? *asked : SP_NULLPTR;
      spn_merged_options_t merged = sp_zero;
      spn_pkg_options_merge(mem, node, &session->profile, session->pkg->config, node_requests, &merged);
      if (!sp_da_empty(merged.violations)) {
        spn_err_t first = SPN_OK;
        sp_da_for(merged.violations, vt) {
          spn_err_t kind = spn_err_emit(session->ctx, violation_err(mem, merged.violations[vt]));
          if (!first) {
            first = kind;
          }
        }
        return first;
      }

      sp_ht_insert(session->options, node->id, merged.options);
    }

    bool pruned = false;
    bool missing = false;
    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, node->id);
      spn_resolved_options_t* resolved = sp_ht_getp(session->options, node->id);
      sp_assert(resolved);

      spn_when_env_t env = sp_zero;
      spn_when_env_from_profile(mem, &session->profile, &env);
      spn_when_env_add_options(&env, resolved);

      sp_da_for(loaded->info->deps, dt) {
        spn_requested_dep_t* dep = &loaded->info->deps[dt];
        if (sp_da_empty(dep->when.clauses)) {
          continue;
        }
        spn_resolved_dep_t* edge = node_find_edge(node, sp_intern_get_or_insert(session->ctx->intern, dep->qualified), dep->kind);
        bool expected = spn_when_eval(&dep->when, &env);
        if (expected && !edge) {
          missing = true;
          if (sp_str_empty(missing_pkg)) {
            missing_pkg = node->name;
            missing_dep = spn_pkg_name_from_qualified(dep->qualified).name;
          }
        }
        if (!expected && edge) {
          node_prune_edge(node, edge);
          pruned = true;
        }
      }
    }

    if (pruned) {
      sweep_unreachable(session);
      continue;
    }
    if (missing) {
      if (session->gates.resolves < SPN_GATE_MAX_RESOLVES) {
        session->gates.resolves++;
        *reresolve = true;
        return SPN_OK;
      }
      return spn_err_emit(session->ctx, (spn_err_union_t) {
        .kind = SPN_ERR_OPTION_LATE_GATE,
        .option = {
          .pkg = missing_pkg,
          .a = { .kind = SPN_OPTION_SETTER_CONSUMER, .name = missing_dep },
        },
      });
    }
    break;
  }

  spn_try(validate_config_keys(session));
  return SPN_OK;
}
