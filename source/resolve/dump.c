#include "resolve/dump.h"

#include "error/error.h"
#include "intern/intern.h"
#include "toml/issue.h"
#include "pkg/id.h"
#include "semver/compare.h"
#include "semver/convert.h"

typedef struct {
  sp_str_t namespace;
  sp_str_t name;
  spn_semver_t version;
  sp_hash_t hash;
} spn_dump_key_t;

typedef struct {
  spn_dump_key_t key;
  const spn_resolved_pkg_t* node;
} spn_dump_pkg_t;

typedef struct {
  spn_dump_key_t key;
  spn_dep_kind_t kind;
  spn_dep_edge_t edge;
  bool private;
} spn_dump_edge_t;

static sp_str_t dump_hash_str(sp_mem_t mem, sp_hash_t hash) {
  return sp_fmt(mem, "{:0>16x}", sp_fmt_uint(hash)).value;
}

static spn_dump_key_t dump_key(sp_intern_t* intern, spn_pkg_id_t id) {
  spn_pkg_name_t name = spn_pkg_name_from_qualified(sp_intern_str_from_id(intern, id.qualified));
  return (spn_dump_key_t) {
    .namespace = name.namespace,
    .name = name.name,
    .version = id.version,
    .hash = id.hash,
  };
}

static s32 dump_key_cmp(const spn_dump_key_t* lhs, const spn_dump_key_t* rhs) {
  s32 order = sp_str_compare_alphabetical(lhs->namespace, rhs->namespace);
  if (order) return order;
  order = sp_str_compare_alphabetical(lhs->name, rhs->name);
  if (order) return order;
  order = spn_semver_cmp(lhs->version, rhs->version);
  if (order) return order;
  if (lhs->hash != rhs->hash) return lhs->hash < rhs->hash ? -1 : 1;
  return 0;
}

static s32 sort_pkg(const void* a, const void* b) {
  return dump_key_cmp(&((const spn_dump_pkg_t*)a)->key, &((const spn_dump_pkg_t*)b)->key);
}

static s32 sort_edge(const void* a, const void* b) {
  const spn_dump_edge_t* lhs = (const spn_dump_edge_t*)a;
  const spn_dump_edge_t* rhs = (const spn_dump_edge_t*)b;
  s32 order = dump_key_cmp(&lhs->key, &rhs->key);
  if (order) return order;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->edge != rhs->edge) return lhs->edge < rhs->edge ? -1 : 1;
  if (lhs->private != rhs->private) return lhs->private < rhs->private ? -1 : 1;
  return 0;
}

static s32 sort_request(const void* a, const void* b) {
  const spn_cg_resolve_request_t* lhs = (const spn_cg_resolve_request_t*)a;
  const spn_cg_resolve_request_t* rhs = (const spn_cg_resolve_request_t*)b;
  s32 order = sp_str_compare_alphabetical(lhs->namespace, rhs->namespace);
  if (order) return order;
  order = sp_str_compare_alphabetical(lhs->name, rhs->name);
  if (order) return order;
  order = sp_str_compare_alphabetical(lhs->version, rhs->version);
  if (order) return order;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->source != rhs->source) return lhs->source < rhs->source ? -1 : 1;
  return 0;
}

static spn_cg_resolve_ref_t dump_ref(sp_mem_t mem, const spn_dump_key_t* key) {
  return (spn_cg_resolve_ref_t) {
    .namespace = key->namespace,
    .name = key->name,
    .version = spn_semver_to_str(mem, key->version),
    .hash = dump_hash_str(mem, key->hash),
  };
}

static void dump_error_pkg(spn_cg_resolve_error_t* error, spn_pkg_name_t id) {
  error->namespace = id.namespace;
  error->name = id.name;
}

static void dump_error_request(sp_mem_t mem, spn_cg_resolve_error_t* error, const spn_requested_dep_t* request) {
  dump_error_pkg(error, spn_pkg_name_from_qualified(request->qualified));
  if (request->source == SPN_PKG_SOURCE_INDEX) {
    error->range = spn_semver_range_to_str(mem, request->index.range);
  }
}

static spn_cg_resolve_error_t dump_error(sp_mem_t mem, const spn_resolve_err_t* err) {
  spn_cg_resolve_error_t error = {
    .kind = sp_cstr_as_str(spn_err_to_str(err->kind)),
  };

  switch (err->kind) {
    case SPN_ERR_PKG_UNKNOWN: {
      dump_error_request(mem, &error, &err->unknown.request);
      break;
    }
    case SPN_ERR_PKG_NO_MATCH: {
      dump_error_request(mem, &error, &err->unsatisfiable.request);
      error.requester = err->unsatisfiable.requester;
      if (err->unsatisfiable.conflict) {
        error.selected = spn_semver_to_str(mem, err->unsatisfiable.selected);
      }
      break;
    }
    case SPN_ERR_DEP_CYCLE: {
      dump_error_pkg(&error, err->circular.id);
      break;
    }
    case SPN_ERR_UNIT_CYCLE: {
      dump_error_pkg(&error, err->unit_cycle.id);
      error.version = spn_semver_to_str(mem, err->unit_cycle.version);
      break;
    }
    case SPN_ERR_DYNAMIC_DUPLICATE: {
      dump_error_pkg(&error, err->dynamic_dup.id);
      error.versions = sp_da_new(mem, sp_str_t);
      sp_da_push(error.versions, spn_semver_to_str(mem, err->dynamic_dup.low));
      sp_da_push(error.versions, spn_semver_to_str(mem, err->dynamic_dup.high));
      break;
    }
    case SPN_ERR_RESOLVE_TOO_COMPLEX: {
      dump_error_pkg(&error, err->too_complex.id);
      break;
    }
    case SPN_ERR_DEP_MANIFEST: {
      dump_error_pkg(&error, spn_pkg_name_from_qualified(err->manifest.name));
      error.detail = err->manifest.error;
      if (sp_str_empty(error.detail)) {
        error.detail = spn_codegen_issues_message(mem, err->manifest.issues);
      }
      break;
    }
    default: {
      break;
    }
  }

  return error;
}

spn_cg_resolve_t spn_resolve_dump(sp_mem_t mem, sp_intern_t* intern, const spn_resolve_query_t* query) {
  spn_cg_resolve_t out = {
    .version = SPN_RESOLVE_DUMP_VERSION,
    .requests = sp_da_new(mem, spn_cg_resolve_request_t),
    .packages = sp_da_new(mem, spn_cg_resolve_pkg_t),
  };

  sp_da_for(query->reqs, it) {
    const spn_requested_dep_t* req = &query->reqs[it];
    spn_pkg_name_t name = spn_pkg_name_from_qualified(req->qualified);
    spn_cg_resolve_request_t request = {
      .namespace = name.namespace,
      .name = name.name,
      .kind = req->kind,
      .source = req->source,
    };
    if (req->source == SPN_PKG_SOURCE_INDEX) {
      request.version = spn_semver_range_to_str(mem, req->index.range);
    }
    sp_da_push(out.requests, request);
  }
  sp_da_sort(out.requests, sort_request);

  if (!sp_da_empty(query->errors)) {
    out.errors = sp_da_new(mem, spn_cg_resolve_error_t);
    sp_da_for(query->errors, it) {
      sp_da_push(out.errors, dump_error(mem, &query->errors[it]));
    }
    return out;
  }

  sp_da(spn_dump_pkg_t) pkgs = sp_da_new(mem, spn_dump_pkg_t);
  sp_ht_for_kv(query->result, it) {
    sp_da_push(pkgs, ((spn_dump_pkg_t) {
      .key = dump_key(intern, it.val->id),
      .node = it.val,
    }));
  }
  sp_da_sort(pkgs, sort_pkg);

  sp_da_for(pkgs, it) {
    const spn_resolved_pkg_t* node = pkgs[it].node;

    sp_da(spn_dump_edge_t) edges = sp_da_new(mem, spn_dump_edge_t);
    sp_da_for(node->edges, jt) {
      const spn_resolved_dep_t* dep = &node->edges[jt];
      sp_da_push(edges, ((spn_dump_edge_t) {
        .key = dump_key(intern, dep->id),
        .kind = dep->kind,
        .edge = dep->edge,
        .private = dep->private,
      }));
    }
    sp_da_sort(edges, sort_edge);

    spn_cg_resolve_pkg_t pkg = {
      .namespace = pkgs[it].key.namespace,
      .name = pkgs[it].key.name,
      .version = spn_semver_to_str(mem, pkgs[it].key.version),
      .hash = dump_hash_str(mem, pkgs[it].key.hash),
      .source = node->source,
      .edges = sp_da_new(mem, spn_cg_resolve_edge_t),
    };

    sp_da_for(edges, jt) {
      spn_cg_resolve_edge_t edge = {
        .to = dump_ref(mem, &edges[jt].key),
        .kind = edges[jt].kind,
        .edge = edges[jt].edge,
      };
      if (edges[jt].private) {
        sp_opt_set(edge.private, true);
      }
      sp_da_push(pkg.edges, edge);
    }

    sp_da_push(out.packages, pkg);
  }

  return out;
}
