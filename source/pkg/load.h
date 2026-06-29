#ifndef SPN_PKG_LOAD_H
#define SPN_PKG_LOAD_H

#include "spn.h"
#include "error/types.h"
#include "forward/types.h"
#include "pkg/types.h"

spn_err_union_t spn_index_load(toml_table_t* toml, sp_str_t parent, u32 index, spn_index_info_t* result);

// The source tree a manifest points at: GIT(url, commit) when it pins a
// separate upstream repo, NONE when its source is inline alongside the
// manifest. The one definition of the split, shared by publish and load.
spn_pkg_tree_t spn_pkg_manifest_source_tree(spn_pkg_info_t* info);

#endif
