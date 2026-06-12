#include "target/mutate.h"

#include "intern/intern.h"

void spn_linkage_set_add(spn_linkage_set_t* set, spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SOURCE: {
      set->source = true;
      break;
    }
    case SPN_LIB_KIND_SHARED: {
      set->shared = true;
      break;
    }
    case SPN_LIB_KIND_STATIC: {
      set->static_lib = true;
      break;
    }
  }
}

bool spn_linkage_set_has(spn_linkage_set_t set, spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SOURCE: {
      return set.source;
    }
    case SPN_LIB_KIND_SHARED: {
      return set.shared;
    }
    case SPN_LIB_KIND_STATIC: {
      return set.static_lib;
    }
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_linkage_t spn_linkage_set_default(spn_linkage_set_t set) {
  if (set.source) {
    return SPN_LIB_KIND_SOURCE;
  }
  if (set.static_lib) {
    return SPN_LIB_KIND_STATIC;
  }
  if (set.shared) {
    return SPN_LIB_KIND_SHARED;
  }

  SP_UNREACHABLE_RETURN(SPN_LIB_KIND_SHARED);
}

void spn_target_add_source_ex(spn_target_info_t* target, sp_str_t source) {
  sp_require(target);
  source = spn_intern(source);
  sp_da_push(target->source, source);
}

void spn_target_add_header_ex(spn_target_info_t* target, sp_str_t header) {
  sp_require(target);
  sp_da_push(target->headers, spn_intern(header));
}

void spn_target_add_include_ex(spn_target_info_t* target, sp_str_t include) {
  sp_require(target);
  sp_da_push(target->include, spn_intern(include));
}

void spn_target_add_define_ex(spn_target_info_t* target, sp_str_t define) {
  sp_require(target);
  sp_da_push(target->define, spn_intern(define));
}

void spn_target_add_dep(spn_target_info_t* target, const c8* dep) {
  spn_target_add_dep_ex(target, sp_str_view(dep));
}

void spn_target_add_dep_ex(spn_target_info_t* target, sp_str_t dep) {
  sp_require(target);
  sp_da_push(target->deps, spn_intern(dep));
}

