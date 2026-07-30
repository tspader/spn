#include "resolve/dump.h"

#include "intern/intern.h"
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
