#include "resolve.h"

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

void spn_resolver_init(spn_resolver_t* resolver, spn_pkg_t* pkg) {
  (void)pkg;

  sp_ht_set_fns(resolver->ranges, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->resolved, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(resolver->visited, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
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
