#include "error/types.h"
#include "event/event.h"
#include "index/cache.h"
#include "pkg/id.h"
#include "resolve/resolve.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "semver/compare.h"
#include "semver/parser.h"
#include "semver/types.h"
#include "session/registry/registry.h"
#include "session/registry/types.h"
#include "spn.h"

static spn_err_t resolve_deps(spn_resolver_t* r, spn_resolve_run_t* resolve, spn_resolved_pkg_t node);

void spn_resolver_init(spn_resolver_t* r, sp_mem_t mem, sp_intern_t* intern, spn_index_cache_t* index, spn_pkg_registry_t* registry, spn_event_buffer_t* events) {
  *r = (spn_resolver_t){
    .mem = mem,
    .intern = intern,
    .index = index,
    .events = events,
    .registry = registry,
  };
}

void spn_resolve_query_add(spn_resolve_query_t* query, spn_requested_pkg_t req) {
  sp_da_push(query->reqs, req);
}

static bool version_in_range(spn_semver_t version, spn_semver_range_t range) {
  return
    spn_semver_satisfies(version, range.low.version, range.low.op) &&
    spn_semver_satisfies(version, range.high.version, range.high.op);
}

static spn_err_t check_cycle(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_requested_pkg_t* request) {
  if (sp_str_ht_exists(run->visited, request->qualified)) {
    spn_event_buffer_push(resolver->events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular.id = spn_qualified_name_to_pkg_id(request->qualified),
    });
    return SPN_ERROR;
  }

  return SPN_OK;
}

static spn_err_t resolve_local_package(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_requested_pkg_t* request) {
  spn_try(check_cycle(resolver, run, request));

  if (sp_str_ht_exists(run->query->result, request->qualified)) {
    return SPN_OK;
  }

  spn_registry_pkg_t* pkg = sp_str_ht_get(*resolver->registry, request->qualified);

  // If the package is local, just load it
  if (!pkg && request->source == SPN_PKG_SOURCE_FILE) {
    spn_registry_err_t err = sp_zero;
    pkg = spn_registry_load_file_pkg(resolver->registry, resolver->mem, resolver->intern, request->qualified, request->file.path, &err);
    if (!pkg) {
      spn_event_buffer_push(resolver->events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_MANIFEST,
        .manifest_err = {
          .name = request->qualified,
          .path = err.manifest,
          .error = err.error,
        }
      });
      return SPN_ERROR;
    }
  }

  if (!pkg) {
    spn_event_buffer_push(resolver->events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
      .unknown.request = *request
    });
    return SPN_ERROR;
  }

  spn_resolved_pkg_t resolved = {
    .qualified = pkg->info->qualified,
    .source = pkg->source,
    .version = pkg->info->version,
  };

  if (pkg->source == SPN_PKG_SOURCE_FILE) {
    resolved.file.path = request->file.path;
  }

  // Looks frivolous, but the point here is that the list of dependencies defined in the manifest is
  // not the same as the list of dependencies we're actually using for this build.
  //
  // Pretty much any  conditional compilation feature will run into this, and this is the place to
  // filter dependencies (e.g. if pkg is only a dependency on aarch64, it would get cut here). The
  // reason it's done here is that:
  //   - It's the earliest place we *can* do it, because resolve is the first place where we have
  //     package dependencies rather than just a list of requested package names
  //   - We don't want to do it later, because then we'd fetch index data for packages that aren't
  //     even going to be included in the build
  sp_str_ht_for_kv(pkg->info->deps, it) {
    sp_da_push(resolved.deps, *it.val);
  }

  sp_str_ht_insert(run->query->result, resolved.qualified, resolved);

  return resolve_deps(resolver, run, resolved);
}

static spn_err_t resolve_index_package(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_requested_pkg_t* request) {
  spn_try(check_cycle(resolver, run, request));

  // If we already resolved a version for this package elsewhere, check if the
  // version we found there satisfies this request, too. If not, backtrack.
  spn_resolved_pkg_t* existing = sp_str_ht_get(run->query->result, request->qualified);
  if (existing) {
    return version_in_range(existing->version, request->index.range) ?
      SPN_OK :
      SPN_ERROR;
  }

  spn_index_pkg_t* pkg = spn_index_cache_get_package(resolver->index, spn_qualified_name_to_pkg_id(request->qualified));
  if (!pkg) {
    spn_event_buffer_push(resolver->events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
      .unknown.request = *request
    });
    return SPN_ERROR;
  }

  // Try this package's versions in reverse order; no other heuristics.
  sp_da_rfor(pkg->releases, it) {
    spn_index_rel_t* release = &pkg->releases[it];
    if (!version_in_range(release->version, request->index.range)) {
      continue;
    }

    // Mark the candidate in the resolved list
    spn_resolved_pkg_t node = {
      .qualified = spn_pkg_id_to_qualified_name(release->id),
      .source = SPN_PKG_SOURCE_INDEX,
      .version = release->version,
      .index = {
        .release = release,
      }
    };
    sp_da_for(release->deps, d) {
      spn_requested_pkg_t dep = {
        .qualified = spn_pkg_id_to_qualified_name(release->deps[d].id),
        .source = SPN_PKG_SOURCE_INDEX,
        .index = {
          .range = spn_semver_parse_range(release->deps[d].version)
        }
      };
      sp_da_push(node.deps, dep);
    }
    sp_str_ht_insert(run->query->result, node.qualified, node);

    // Resolve the subtree rooted at this package
    if (!resolve_deps(resolver, run, node)) {
      return SPN_OK;
    }

    sp_str_ht_erase(run->query->result, node.qualified);
  }

  spn_event_buffer_push(resolver->events, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsatisfiable = {
      .low = *request,
      .high = *request
    }
  });
  return SPN_ERROR;
}

static spn_err_t resolve_deps(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t node) {
  // If we've already seen this package, it must transitively include itself.
  if (sp_str_ht_exists(run->visited, node.qualified)) {
    spn_event_buffer_push(resolver->events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular.id = spn_qualified_name_to_pkg_id(node.qualified),
    });
    return SPN_ERROR;
  }

  sp_str_ht_insert(run->visited, node.qualified, true);

  spn_err_t result = SPN_OK;
  sp_da_for(node.deps, it) {
    spn_requested_pkg_t* dep = &node.deps[it];

    switch (dep->source) {
      case SPN_PKG_SOURCE_INDEX: result = resolve_index_package(resolver, run, dep); break;
      case SPN_PKG_SOURCE_ROOT:
      case SPN_PKG_SOURCE_FILE:  result = resolve_local_package(resolver, run, dep); break;
    }

    if (result) goto done;
  }

done:
  sp_str_ht_erase(run->visited, node.qualified);
  return result;
}

spn_err_t spn_resolve_from_lock_file(spn_resolver_t* resolver, spn_lock_file_t* lock) {
  return SPN_OK;
}

spn_err_t spn_resolve_from_solver(spn_resolver_t* resolver, spn_resolve_query_t* query) {
  spn_resolve_run_t run = {
    .query = query,
  };

  spn_resolved_pkg_t node = {
    .qualified = sp_str_lit(""),
    .deps = query->reqs,
  };

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_err_t err = resolve_deps(resolver, &run, node);
  query->time = sp_tm_read_timer(&timer);
  return err;
}
