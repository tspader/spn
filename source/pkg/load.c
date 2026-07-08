#include "sp.h"
#include "pkg/types.h"

#include "pkg/load.h"

spn_pkg_tree_t spn_pkg_manifest_source_tree(spn_pkg_info_t* info) {
  spn_pkg_metadata_t* meta = sp_ht_getp(info->metadata, info->version);
  if (!sp_str_empty(info->url) && meta && !sp_str_empty(meta->commit)) {
    return (spn_pkg_tree_t) {
      .kind = SPN_PKG_TREE_GIT,
      .git = { .url = info->url, .rev = meta->commit },
    };
  }

  return (spn_pkg_tree_t) { .kind = SPN_PKG_TREE_NONE };
}
