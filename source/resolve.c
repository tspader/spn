#include "resolve.h"
#include "event.h"

spn_pkg_req_t spn_pkg_req_from_str(sp_str_t str) {
  if (sp_str_starts_with(str, sp_str_lit("file://"))) {
    return (spn_pkg_req_t) {
      .kind = SPN_PACKAGE_KIND_FILE,
      .file = sp_str_copy(str)
    };
  }

  return (spn_pkg_req_t) {
    .kind = SPN_PACKAGE_KIND_INDEX,
    .range = spn_semver_range_from_str(str),
  };
}

sp_str_t spn_pkg_req_to_str(spn_pkg_req_t dep) {
  switch (dep.kind) {
    case SPN_PACKAGE_KIND_FILE: {
      return dep.file;
    }
    case SPN_PACKAGE_KIND_INDEX: {
      return spn_semver_range_to_str(dep.range);
    }
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_WORKSPACE: {
      SP_BROKEN();
      break;
    }
    case SPN_PACKAGE_KIND_NONE: {
      SP_UNREACHABLE_RETURN(sp_str_lit(""));
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void spn_resolver_init(spn_resolver_t* r, spn_pkg_t* pkg, spn_pkg_cache_t* cache, spn_pkg_registry_t* registry) {
  r->pkg = pkg;
  r->cache = cache;
  r->registry = registry;
}

spn_resolve_strategy_t spn_resolve_strategy_from_str(sp_str_t str) {
  SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_RESOLVE_STRATEGY_SOLVER);
}

sp_str_t spn_resolve_strategy_to_str(spn_resolve_strategy_t strategy) {
  switch (strategy) {
    SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_err_t spn_app_add_pkg_constraints(spn_app_t* app, spn_pkg_t* pkg) {
  spn_resolver_t* resolver = app->resolver;

  if (sp_ht_key_exists(resolver->visited, pkg->name)) {
    spn_push_event_ex((spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular = {
        .pkg = pkg
      }
    });

    return SPN_ERROR;
  }

  // system deps
  sp_dyn_array_for(pkg->system_deps, i) {
    sp_str_t sys_dep = pkg->system_deps[i];
    bool found = false;
    sp_dyn_array_for(resolver->system_deps, j) {
      if (sp_str_equal(resolver->system_deps[j], sys_dep)) { found = true; break; }
    }
    if (!found) sp_dyn_array_push(resolver->system_deps, sys_dep);
  }

  // prevent circular deps by marking this dep until we're done with the subtree
  sp_str_ht_insert(resolver->visited, pkg->name, true);

  sp_ht_for_kv(pkg->deps, it) {
    spn_pkg_req_t request = *it.val;
    sp_require_as(!sp_str_empty(request.name), SPN_ERROR);

    spn_pkg_t* dep = spn_app_ensure_package(app, request);
    if (!dep) {
      spn_push_event_ex((spn_build_event_t) {
        .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
        .unknown = {
          .request = request
        }
      });

      return SPN_ERROR;
    }

    // recurse
    sp_try(spn_app_add_pkg_constraints(app, dep));

    // add the dependency itself
    if (!sp_ht_key_exists(resolver->ranges, dep->name)) {
      sp_str_ht_insert(resolver->ranges, dep->name, SP_NULLPTR);
    }
    sp_da(spn_resolve_range_t)* ranges = sp_str_ht_get(resolver->ranges, dep->name);

    // collect the range of versions which satisfy the request
    spn_resolve_range_t range = {
      .source = request
    };

    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        u32 num_versions = sp_dyn_array_size(dep->versions);
        if (num_versions != 1) {
          SP_FATAL(
            "Local dependency {:fg brightcyan} has {} versions",
            SP_FMT_STR(dep->name),
            SP_FMT_U32(num_versions)
          );
        }
        sp_opt_set(range.low, 0);
        sp_opt_set(range.high, 0);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        spn_semver_t low = request.range.low.version;
        spn_semver_t high = request.range.high.version;

        sp_dyn_array_for(dep->versions, n) {
          spn_semver_t version = dep->versions[n];

          if (!range.low.some) {
            if (spn_semver_satisfies(version, low, request.range.low.op)) {
              sp_opt_set(range.low, n);
            }
          }

          if (spn_semver_satisfies(version, high, request.range.high.op)) {
            sp_opt_set(range.high, n);
          }
        }

        break;
      }
      case SPN_PACKAGE_KIND_ROOT:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_BROKEN();
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_CASE();
      }
    }

    sp_dyn_array_push(*ranges, range);
  }

  sp_str_ht_erase(resolver->visited, pkg->name);

  return SPN_OK;
}

void spn_app_resolve_from_lock_file(spn_app_t* app) {
  SP_ASSERT(app->lock.some);

  spn_lock_file_t* lock = &app->lock.value;
  sp_ht_for_kv(lock->entries, it) {
    spn_lock_entry_t* entry = it.val;

    spn_pkg_req_t request = {
      .name = *it.key,
      .kind = entry->kind,
      .visibility = entry->visibility,
    };

    if (request.kind == SPN_PACKAGE_KIND_INDEX) {
      request.range = (spn_semver_range_t) {
        .low = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .high = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .mod = SPN_SEMVER_MOD_CMP
      };
    }
    else if (request.kind == SPN_PACKAGE_KIND_FILE) {
      spn_pkg_req_t* dep = sp_ht_getp(app->package.deps, request.name);
      SP_ASSERT(dep);
      SP_ASSERT(dep->kind == SPN_PACKAGE_KIND_FILE);
      request.file = dep->file;
    }

    spn_pkg_t* pkg = spn_app_ensure_package(app, request);

    sp_str_ht_insert(app->resolver->resolved, entry->name, ((spn_resolved_pkg_t) {
      .pkg = pkg,
      .kind = request.kind,
      .version = entry->version
    }));
  }

  sp_ht_for_kv(lock->system_deps, it) {
    sp_da_push(app->resolver->system_deps, *it.key);
  }
}

spn_err_t spn_app_resolve_from_solver(spn_app_t* app) {
  sp_try(spn_app_add_pkg_constraints(app, &app->package));

  sp_str_ht_for_kv(app->resolver->ranges, it) {
    sp_str_t name = *it.key;
    sp_da(spn_resolve_range_t) ranges = *it.val;
    if (sp_da_empty(ranges)) {
      return SPN_ERROR;
    }

    spn_pkg_req_t req_low, req_high = SP_ZERO_INITIALIZE();
    u32 low = 0, high = SP_LIMIT_U32_MAX;
    sp_dyn_array_for(ranges, n) {
      spn_resolve_range_t range = ranges[n];
      SP_ASSERT(range.low.some);
      SP_ASSERT(range.high.some);

      if (sp_opt_get(range.low) >= low) {
        low = sp_opt_get(range.low);
        req_low = range.source;
      }
      if (sp_opt_get(range.high) <= high) {
        high = sp_opt_get(range.high);
        req_high = range.source;
      }
    }

    if (low > high) {
      sp_str_builder_t builder = SP_ZERO_INITIALIZE();
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} cannot be resolved:", SP_FMT_STR(name));
      sp_str_builder_indent(&builder);
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_low.name), SP_FMT_STR(spn_semver_range_to_str(req_low.range)));
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_high.name), SP_FMT_STR(spn_semver_range_to_str(req_high.range)));

      SP_FATAL("{}", SP_FMT_STR(sp_str_builder_to_str(&builder)));
    }


    spn_pkg_t* pkg = spn_app_ensure_package(app, req_high);
    sp_str_ht_insert(app->resolver->resolved, name, ((spn_resolved_pkg_t) {
      .pkg = pkg,
      .version = pkg->versions[high],
      .kind = req_high.kind,
    }));
  }

  return SPN_OK;
}

void spn_app_resolve(spn_app_t* app) {
  spn_resolver_init(app->resolver, &app->package, &app->cache, &app->registry);

  switch (app->lock.some) {
    case SP_OPT_SOME: {
      spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_LOCK_FILE
        }
      });

      spn_app_resolve_from_lock_file(app);
      break;
    }
    case SP_OPT_NONE: {
      spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_SOLVER
        }
      });

      spn_app_resolve_from_solver(app);
      break;
    }
  }
}
