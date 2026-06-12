#include "target/select.h"

#include "target/mutate.h"

static spn_err_t select_first_supported(spn_linkage_set_t supported, const spn_linkage_t* order, u32 len, spn_linkage_t* out) {
  sp_for(it, len) {
    if (spn_linkage_set_has(supported, order[it])) {
      *out = order[it];
      return SPN_OK;
    }
  }

  return SPN_ERROR;
}

spn_err_t spn_target_select_lib_kind(spn_target_info_t* info, spn_kind_query_t query, spn_linkage_t* out) {
  spn_linkage_set_t supported = info->linkages;

  if (query.config.some) {
    if (!spn_linkage_set_has(supported, query.config.value)) {
      return SPN_ERROR;
    }

    *out = query.config.value;
    return SPN_OK;
  }

  // The profile's linkage is a constraint, not a preference: a static or source build promises
  // no runtime library dependencies, so shared libs are excluded outright. Within whatever is
  // allowed, source is always the cheapest and safest choice, so the order is fixed.
  switch (query.linkage) {
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SOURCE: {
      static const spn_linkage_t order [] = { SPN_LIB_KIND_SOURCE, SPN_LIB_KIND_STATIC };
      return select_first_supported(supported, order, SP_CARR_LEN(order), out);
    }
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_NONE: {
      static const spn_linkage_t order [] = { SPN_LIB_KIND_SOURCE, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED };
      return select_first_supported(supported, order, SP_CARR_LEN(order), out);
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}
