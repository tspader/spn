#include "err.h"
#include "event/event.h"
#include "index/cache.h"
#include "pkg/id.h"
#include "resolve/resolve.h"
#include "semver/compare.h"
#include "semver/parser.h"

void spn_resolver_init(spn_resolver_t* r, spn_index_cache_t* index, spn_pkg_t* pkg, spn_event_buffer_t* events) {
  *r = (spn_resolver_t){ .pkg = pkg, .index = index, .events = events };
}

static bool version_in_range(spn_semver_t version, spn_semver_range_t range) {
  return spn_semver_satisfies(version, range.low.version, range.low.op)
      && spn_semver_satisfies(version, range.high.version, range.high.op);
}

static bool system_dep_exists(spn_resolver_t* r, sp_str_t dep) {
  sp_da_for(r->system_deps, it) {
    if (sp_str_equal(r->system_deps[it], dep)) {
      return true;
    }
  }
  return false;
}

static spn_err_t resolve_node(spn_resolver_t* r, spn_resolve_node_t node);

static spn_err_t resolve_package(spn_resolver_t* r, spn_pkg_req_t req) {
  sp_str_t qualified = spn_pkg_id_to_qualified_name(req.id);

  spn_resolved_pkg_t* existing = sp_str_ht_get(r->resolved, qualified);
  if (existing) {
    if (sp_ht_key_exists(r->visited, qualified)) {
      spn_event_buffer_push_ex(r->events, r->pkg, SP_NULLPTR, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
        .circular.id = req.id
      });
      return SPN_ERROR;
    }
    if (version_in_range(existing->version, req.range)) {
      return SPN_OK;
    }
    return SPN_ERROR;
  }

  spn_index_pkg_t* pkg = spn_index_cache_get_package(r->index, req.id);
  if (!pkg) {
    spn_event_buffer_push_ex(r->events, r->pkg, SP_NULLPTR, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
      .unknown.request = req
    });
    return SPN_ERROR;
  }

  u32 saved_resolutions = sp_da_size(r->resolution_order);
  u32 saved_system_deps = sp_da_size(r->system_deps);

  for (s32 it = (s32)sp_da_size(pkg->releases) - 1; it >= 0; it--) {
    spn_index_rel_t* candidate = &pkg->releases[it];

    if (!version_in_range(candidate->version, req.range)) {
      continue;
    }

    sp_str_ht_insert(r->resolved, qualified, ((spn_resolved_pkg_t) {
      .version = candidate->version,
      .kind = req.kind,
      .release = candidate,
    }));
    sp_da_push(r->resolution_order, qualified);

    spn_resolve_node_t child = { .id = pkg->id };
    sp_da_for(candidate->deps, d) {
      sp_da_push(child.deps.pkg, ((spn_pkg_req_t) {
        .id = candidate->deps[d].id,
        .kind = SPN_PACKAGE_KIND_INDEX,
        .range = spn_semver_parse_range(candidate->deps[d].version)
      }));
    }

    spn_err_t err = resolve_node(r, child);
    if (err == SPN_OK) {
      return SPN_OK;
    }

    while (sp_da_size(r->resolution_order) > saved_resolutions) {
      sp_str_t key = *sp_da_back(r->resolution_order);
      sp_str_ht_erase(r->resolved, key);
      sp_da_pop(r->resolution_order);
    }

    while (sp_da_size(r->system_deps) > saved_system_deps) {
      sp_da_pop(r->system_deps);
    }
  }

  spn_event_buffer_push_ex(r->events, r->pkg, SP_NULLPTR, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsatisfiable = {
      .low = req,
      .high = req
    }
  });
  return SPN_ERROR;
}

static spn_err_t resolve_node(spn_resolver_t* r, spn_resolve_node_t node) {
  sp_str_t qualified = spn_pkg_id_to_qualified_name(node.id);

  if (sp_ht_key_exists(r->visited, qualified)) {
    spn_event_buffer_push_ex(r->events, r->pkg, SP_NULLPTR, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular.id = node.id
    });
    return SPN_ERROR;
  }

  sp_da_for(node.deps.system, it) {
    if (!system_dep_exists(r, node.deps.system[it])) {
      sp_da_push(r->system_deps, node.deps.system[it]);
    }
  }

  sp_str_ht_insert(r->visited, qualified, true);

  sp_da_for(node.deps.pkg, it) {
    spn_pkg_req_t req = node.deps.pkg[it];
    switch (req.kind) {
      case SPN_PACKAGE_KIND_INDEX: {
        spn_err_t err = resolve_package(r, req);
        if (err != SPN_OK) {
          sp_str_ht_erase(r->visited, qualified);
          return err;
        }
        break;
      }
      case SPN_PACKAGE_KIND_FILE:
      case SPN_PACKAGE_KIND_ROOT: {
        break;
      }
    }
  }

  sp_str_ht_erase(r->visited, qualified);
  return SPN_OK;
}

spn_err_t spn_resolve_from_lock_file(spn_resolver_t* resolver, spn_lock_file_t* lock) {
  sp_ht_for_kv(lock->entries, it) {
    spn_lock_entry_t* entry = it.val;

    if (entry->kind != SPN_PACKAGE_KIND_INDEX) continue;

    spn_pkg_id_t id = spn_qualified_name_to_pkg_id(*it.key);
    sp_str_t qualified = spn_pkg_id_to_qualified_name(id);

    spn_index_rel_t* release = spn_index_cache_get_release(resolver->index, id, entry->version);

    if (!release) {
      release = sp_alloc_type(spn_index_rel_t);
      *release = (spn_index_rel_t) {
        .id = id,
        .version = entry->version,
        .source = { .url = entry->source.url, .rev = entry->source.rev, .dir = entry->source.dir },
        .manifest = { .url = entry->manifest.url, .rev = entry->manifest.rev, .dir = entry->manifest.dir },
        .paths = { .manifest = entry->paths.manifest, .script = entry->paths.script },
      };
    }

    sp_str_ht_insert(resolver->resolved, qualified, ((spn_resolved_pkg_t) {
      .version = entry->version,
      .kind = entry->kind,
      .release = release,
    }));
  }

  sp_ht_for_kv(lock->system_deps, it) {
    sp_da_push(resolver->system_deps, *it.key);
  }

  return SPN_OK;
}

spn_err_t spn_resolve_from_solver(spn_resolver_t* r) {
  spn_resolve_node_t node = {
    .id.name = r->pkg->name,
    .version = r->pkg->version,
    .deps = {
      .system = r->pkg->system_deps,
    }
  };
  sp_ht_for_kv(r->pkg->deps, it) {
    sp_da_push(node.deps.pkg, *it.val);
  }

  return resolve_node(r, node);
}
