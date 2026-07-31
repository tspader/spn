#include "index/release.h"

#include "pkg/id.h"
#include "pkg/pkg.h"
#include "target/mutate.h"

static spn_index_dep_kind_t dep_kind_to_index(spn_dep_kind_t kind) {
  switch (kind) {
    case SPN_DEP_KIND_PACKAGE: return SPN_INDEX_DEP_NORMAL;
    case SPN_DEP_KIND_BUILD:   return SPN_INDEX_DEP_BUILD;
    case SPN_DEP_KIND_TEST:    return SPN_INDEX_DEP_TEST;
  }
  sp_unreachable_return(SPN_INDEX_DEP_NORMAL);
}

spn_err_union_t spn_index_release_from_pkg(sp_mem_t mem, spn_pkg_info_t* info, spn_index_release_t* release) {
  *release = (spn_index_release_t) {
    .id = spn_pkg_name_from_qualified(info->qualified),
    .version = info->version,
    .source = spn_pkg_upstream(info),
    .paths = {
      .manifest = sp_str_lit("spn.toml"),
      .script = sp_str_lit("spn.c"),
    },
    .options = info->options,
  };

  sp_da_init(mem, release->deps);
  sp_da_init(mem, release->targets);

  sp_da_for(info->deps, it) {
    spn_requested_dep_t* req = &info->deps[it];
    if (req->source != SPN_PKG_SOURCE_INDEX) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_INDEX_PATH_DEP,
        .pkg = { .name = info->qualified, .requested = req->qualified },
      };
    }

    sp_da_push(release->deps, ((spn_index_dep_t) {
      .kind = dep_kind_to_index(req->kind),
      .private = req->private,
      .id = spn_pkg_name_from_qualified(req->qualified),
      .range = req->index.range,
      .when = req->when,
      .options = req->options,
    }));
  }

  sp_str_om_for(info->libs, it) {
    spn_target_info_t* lib = sp_str_om_at(info->libs, it);
    spn_index_target_t target = { .name = lib->name };
    sp_da_init(mem, target.linkages);

    const spn_linkage_t kinds [] = { SPN_LIB_KIND_SOURCE, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED, SPN_LIB_KIND_OBJECT };
    sp_carr_for(kinds, kind) {
      if (spn_linkage_set_has(lib->linkages, kinds[kind])) {
        sp_da_push(target.linkages, kinds[kind]);
      }
    }

    sp_da_push(release->targets, target);
  }

  return spn_result(SPN_OK);
}
