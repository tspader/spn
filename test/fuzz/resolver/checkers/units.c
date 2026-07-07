#include "fuzz.h"

#include "semver/compare.h"
#include "sp/str.h"

typedef struct {
  spn_resolved_pkg_t* node;
  s32 pkg;
} fz_instance_t;

typedef sp_da(fz_instance_t) fz_instance_arr_t;

typedef struct {
  sp_hash_t label;
  u32 kind;
  u32 private;
} fz_label_record_t;

static s32 fz_find_inst(fz_instance_arr_t arr, spn_pkg_id_t id) {
  sp_da_for(arr, it) {
    spn_pkg_id_t other = arr[it].node->id;
    if (other.qualified == id.qualified && spn_semver_eq(other.version, id.version) && other.hash == id.hash) {
      return (s32)it;
    }
  }
  return -1;
}

static fz_release_t* fz_inst_release(fz_universe_t* u, fz_instance_t* instance) {
  sp_da_for(u->pkgs[instance->pkg].releases, rt) {
    if (spn_semver_eq(u->pkgs[instance->pkg].releases[rt].version, instance->node->version)) {
      return &u->pkgs[instance->pkg].releases[rt];
    }
  }
  return SP_NULLPTR;
}

static spn_dep_kind_t fz_dep_kind(spn_index_dep_kind_t kind) {
  switch (kind) {
    case SPN_INDEX_DEP_NORMAL: return SPN_DEP_KIND_PACKAGE;
    case SPN_INDEX_DEP_BUILD:  return SPN_DEP_KIND_BUILD;
    case SPN_INDEX_DEP_TEST:   return SPN_DEP_KIND_TEST;
  }
  sp_unreachable_return(SPN_DEP_KIND_PACKAGE);
}

// Mirrors classify_dep against the universe instead of the resolver's own
// bookkeeping: build deps cross a process boundary, test deps survive only on
// the root, and a private dep leaves the scope iff its owner is shared
static spn_dep_edge_t fz_classify(fz_universe_t* u, fz_instance_t* owner, fz_dep_t dep) {
  switch (dep.kind) {
    case SPN_INDEX_DEP_BUILD: {
      return SPN_DEP_EDGE_PROCESS;
    }
    case SPN_INDEX_DEP_TEST: {
      return owner->pkg < 0 ? SPN_DEP_EDGE_PROCESS : SPN_DEP_EDGE_PRUNED;
    }
    case SPN_INDEX_DEP_NORMAL: {
      break;
    }
  }
  if (dep.private && owner->pkg >= 0 && u->pkgs[owner->pkg].shared) {
    return SPN_DEP_EDGE_PRIVATE;
  }
  return SPN_DEP_EDGE_SCOPE;
}

static fz_err_t fz_check_edges(fz_universe_t* u, fz_instance_arr_t instances, s32 index) {
  fz_instance_t* instance = &instances[index];
  sp_da(fz_dep_t) deps = instance->pkg < 0 ? u->roots : fz_inst_release(u, instance)->deps;

  u64 expected = 0;
  sp_da_for(deps, dt) {
    fz_dep_t dep = deps[dt];
    spn_dep_edge_t edge_kind = fz_classify(u, instance, dep);
    spn_dep_kind_t kind = fz_dep_kind(dep.kind);
    if (edge_kind == SPN_DEP_EDGE_PRUNED) {
      continue;
    }

    bool counted = false;
    for (u64 jt = 0; jt < dt; jt++) {
      fz_dep_t prior = deps[jt];
      if (prior.pkg == dep.pkg && fz_dep_kind(prior.kind) == kind && prior.private == dep.private && fz_classify(u, instance, prior) != SPN_DEP_EDGE_PRUNED) {
        counted = true;
        break;
      }
    }
    if (!counted) {
      expected++;
    }

    spn_resolved_dep_t* held = SP_NULLPTR;
    sp_da_for(instance->node->edges, et) {
      spn_resolved_dep_t* edge = &instance->node->edges[et];
      if (edge->kind != kind || edge->private != dep.private) {
        continue;
      }
      s32 target = fz_find_inst(instances, edge->id);
      if (target < 0 || instances[target].pkg != (s32)dep.pkg) {
        continue;
      }
      held = edge;
      break;
    }
    if (!held) {
      return FZ_ERR_EDGE_MISSING;
    }
    if (held->edge != edge_kind) {
      return FZ_ERR_EDGE_MISCLASSIFIED;
    }
    if (!fz_range_sat(dep, held->id.version)) {
      return FZ_ERR_EDGE_OUT_OF_RANGE;
    }
  }

  if (expected != sp_da_size(instance->node->edges)) {
    return FZ_ERR_EDGE_EXTRA;
  }
  return FZ_OK;
}

static fz_err_t fz_check_unit(sp_mem_t mem, fz_universe_t* u, fz_instance_arr_t instances, sp_da(s32) roots) {
  s32 named[FZ_MAX_PKGS];
  sp_carr_for(named, it) {
    named[it] = -1;
  }

  sp_da(s32) work = sp_da_new(mem, s32);
  sp_da(u8) visited = sp_da_new(mem, u8);
  sp_da_for(instances, it) {
    sp_da_push(visited, 0);
  }
  sp_da_for(roots, it) {
    sp_da_push(work, roots[it]);
  }

  while (!sp_da_empty(work)) {
    s32 index = *sp_da_back(work);
    sp_da_pop(work);
    if (visited[index]) continue;
    visited[index] = 1;

    fz_instance_t* inst = &instances[index];
    if (inst->pkg >= 0) {
      if (named[inst->pkg] >= 0 && named[inst->pkg] != index) {
        return FZ_ERR_UNIT_DUPLICATE;
      }
      named[inst->pkg] = index;
    }

    sp_da_for(inst->node->edges, et) {
      if (inst->node->edges[et].edge != SPN_DEP_EDGE_SCOPE) continue;
      sp_da_push(work, fz_find_inst(instances, inst->node->edges[et].id));
    }
  }

  return FZ_OK;
}

static fz_err_t fz_check_process(sp_mem_t mem, fz_universe_t* u, fz_instance_arr_t instances, s32 root) {
  s32 named[FZ_MAX_PKGS];
  sp_carr_for(named, it) {
    named[it] = -1;
  }

  sp_da(s32) work = sp_da_new(mem, s32);
  sp_da(u8) visited = sp_da_new(mem, u8);
  sp_da_for(instances, it) {
    sp_da_push(visited, 0);
  }
  sp_da_push(work, root);

  while (!sp_da_empty(work)) {
    s32 index = *sp_da_back(work);
    sp_da_pop(work);
    if (visited[index]) continue;
    visited[index] = 1;

    fz_instance_t* inst = &instances[index];
    if (inst->pkg >= 0 && u->pkgs[inst->pkg].shared) {
      if (named[inst->pkg] >= 0 && named[inst->pkg] != index) {
        return FZ_ERR_SHARED_DUP_MISSED;
      }
      named[inst->pkg] = index;
    }

    sp_da_for(inst->node->edges, et) {
      spn_dep_edge_t kind = inst->node->edges[et].edge;
      if (kind != SPN_DEP_EDGE_SCOPE && kind != SPN_DEP_EDGE_PRIVATE) continue;
      sp_da_push(work, fz_find_inst(instances, inst->node->edges[et].id));
    }
  }

  return FZ_OK;
}

static bool fz_acyclic_at(fz_instance_arr_t instances, u8* states, s32 index) {
  if (states[index] == 2) return true;
  if (states[index] == 1) return false;
  states[index] = 1;

  sp_da_for(instances[index].node->edges, et) {
    s32 target = fz_find_inst(instances, instances[index].node->edges[et].id);
    if (!fz_acyclic_at(instances, states, target)) {
      return false;
    }
  }

  states[index] = 2;
  return true;
}

static s32 fz_sort_label_record(const void* a, const void* b) {
  const fz_label_record_t* lhs = (const fz_label_record_t*)a;
  const fz_label_record_t* rhs = (const fz_label_record_t*)b;
  if (lhs->label != rhs->label) return lhs->label < rhs->label ? -1 : 1;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->private != rhs->private) return lhs->private < rhs->private ? -1 : 1;
  return 0;
}

static sp_hash_t fz_label(sp_mem_t mem, fz_instance_arr_t instances, sp_hash_t* labels, u8* done, s32 index) {
  if (done[index]) {
    return labels[index];
  }

  spn_resolved_pkg_t* node = instances[index].node;
  sp_da(fz_label_record_t) records = sp_da_new(mem, fz_label_record_t);
  sp_da_for(node->edges, et) {
    spn_resolved_dep_t* edge = &node->edges[et];
    sp_da_push(records, ((fz_label_record_t) {
      .label = fz_label(mem, instances, labels, done, fz_find_inst(instances, edge->id)),
      .kind = (u32)edge->kind,
      .private = (u32)edge->private,
    }));
  }
  sp_da_sort(records, fz_sort_label_record);

  sp_hash_t label = sp_hash_str(node->qualified);
  label = sp_hash_bytes(&node->version, sizeof(spn_semver_t), label);
  if (!sp_da_empty(records)) {
    label = sp_hash_bytes(records, sp_da_size(records) * sizeof(fz_label_record_t), label);
  }

  labels[index] = label;
  done[index] = 1;
  return label;
}

fz_err_t fz_check_units(sp_mem_t mem, fz_universe_t* u, spn_resolve_query_t* query) {
  fz_instance_arr_t instances = sp_da_new(mem, fz_instance_t);
  s32 root = -1;

  sp_ht_for_kv(query->result, it) {
    spn_resolved_pkg_t* node = it.val;
    fz_instance_t instance = { .node = node, .pkg = -1 };

    if (sp_str_equal(node->qualified, sp_str_view(fz_root_qualified))) {
      root = sp_da_size(instances);
    }
    else {
      instance.pkg = fz_pkg_from_qualified(u, node->qualified);
      must(instance.pkg >= 0, FZ_ERR_SOLVE_FOREIGN_PKG);
      must(fz_inst_release(u, &instance), FZ_ERR_SOLVE_FOREIGN_VERSION);
    }

    sp_da_push(instances, instance);
  }
  must(root >= 0, FZ_ERR_ROOT_MISSING);

  sp_da_for(instances, it) {
    sp_da_for(instances[it].node->edges, et) {
      if (fz_find_inst(instances, instances[it].node->edges[et].id) < 0) {
        return FZ_ERR_DEP_UNRESOLVED;
      }
    }
  }

  sp_da_for(instances, it) {
    try(fz_check_edges(u, instances, it));
  }

  sp_da(u8) states = sp_da_new(mem, u8);
  sp_da_for(instances, it) {
    sp_da_push(states, 0);
  }
  sp_da_for(instances, it) {
    must(fz_acyclic_at(instances, states, it), FZ_ERR_GRAPH_CYCLE);
  }

  sp_da(s32) roots = sp_da_new(mem, s32);
  sp_da_push(roots, root);
  try(fz_check_unit(mem, u, instances, roots));

  sp_da_for(instances, it) {
    sp_da(s32) privates = sp_da_new(mem, s32);
    sp_da_for(instances[it].node->edges, et) {
      spn_resolved_dep_t* edge = &instances[it].node->edges[et];
      switch (edge->edge) {
        case SPN_DEP_EDGE_PROCESS: {
          sp_da(s32) process = sp_da_new(mem, s32);
          sp_da_push(process, fz_find_inst(instances, edge->id));
          try(fz_check_unit(mem, u, instances, process));
          break;
        }
        case SPN_DEP_EDGE_PRIVATE: {
          sp_da_push(privates, fz_find_inst(instances, edge->id));
          break;
        }
        case SPN_DEP_EDGE_SCOPE:
        case SPN_DEP_EDGE_PRUNED: {
          break;
        }
      }
    }
    if (!sp_da_empty(privates)) {
      try(fz_check_unit(mem, u, instances, privates)); // :3
    }
  }

  try(fz_check_process(mem, u, instances, root));
  sp_da_for(instances, it) {
    sp_da_for(instances[it].node->edges, j) {
      if (instances[it].node->edges[j].edge != SPN_DEP_EDGE_PROCESS) continue;
      try(fz_check_process(mem, u, instances, fz_find_inst(instances, instances[it].node->edges[j].id)));
    }
  }

  sp_da(sp_hash_t) labels = sp_da_new(mem, sp_hash_t);
  sp_da(u8) done = sp_da_new(mem, u8);
  sp_da_for(instances, it) {
    sp_da_push(labels, 0);
    sp_da_push(done, 0);
  }
  sp_da_for(instances, it) {
    fz_label(mem, instances, labels, done, it);
  }
  sp_da_for(instances, it) {
    for (u64 jt = it + 1; jt < sp_da_size(instances); jt++) {
      if (instances[it].pkg < 0 || instances[it].pkg != instances[jt].pkg) continue;
      if (!spn_semver_eq(instances[it].node->version, instances[jt].node->version)) continue;
      if (labels[it] == labels[jt]) {
        return FZ_ERR_IDENTITY_SPLIT;
      }
    }
  }

  return FZ_OK;
}
