#include "sp.h"
#include "intern/types.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/types.h"

#include "event/event.h"
#include "event/types.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/options.h"
#include "session/session.h"
#include "when/when.h"

#define SPN_GATE_MAX_RESOLVES 4

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

static spn_err_union_t validate_config_keys(spn_session_t* session) {
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
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_OPTION,
        .option = {
          .err = SPN_OPTION_ERR_UNKNOWN_PKG,
          .pkg = entry->key,
        },
      });
      return spn_err_reported(SPN_ERR_OPTION);
    }
  }
  return spn_result(SPN_OK);
}

spn_err_union_t spn_session_apply_options(spn_session_t* session) {
  sp_mem_t mem = session->mem;
  sp_str_t missing_pkg = sp_zero;
  sp_str_t missing_dep = sp_zero;

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
        spn_resolved_dep_t* edge = node_find_edge(node, sp_intern_get_or_insert(session->intern, dep->qualified), dep->kind);
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
        sp_intern_id_t seed_key = target ? target->id.qualified : sp_intern_get_or_insert(session->intern, dep->qualified);
        if (!sp_ht_getp(session->gates.seeds, seed_key)) {
          sp_ht_insert(session->gates.seeds, seed_key, sp_da_new(mem, spn_option_request_t));
        }
        sp_da_push(*sp_ht_getp(session->gates.seeds, seed_key), request);
      }
    }

    sp_ht_for_kv(session->resolve, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_option_requests_t* asked = sp_ht_getp(requests, node->id);

      spn_resolved_options_t resolved = sp_zero;
      spn_try_as(spn_pkg_options_merge(
        mem,
        node,
        &session->profile,
        session->pkg->config,
        asked ? *asked : SP_NULLPTR,
        session->events,
        &resolved), spn_err_reported(SPN_ERR_OPTION));

      sp_ht_insert(session->options, node->id, resolved);
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
        spn_resolved_dep_t* edge = node_find_edge(node, sp_intern_get_or_insert(session->intern, dep->qualified), dep->kind);
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
        session->gates.reresolve = true;
        return spn_result(SPN_OK);
      }
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_OPTION,
        .option = {
          .err = SPN_OPTION_ERR_LATE_GATE,
          .pkg = missing_pkg,
          .a = { .kind = SPN_OPTION_SETTER_CONSUMER, .name = missing_dep },
        },
      });
      return spn_err_reported(SPN_ERR_OPTION);
    }
    break;
  }

  try_union(validate_config_keys(session));
  return spn_result(SPN_OK);
}
