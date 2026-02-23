#include "index.h"
#include "ctx.h"

sp_str_t spn_index_get_path(spn_index_t* index) {
  switch (index->kind) {
    case SPN_INDEX_WORKSPACE: {
      return sp_fs_join_path(spn_app_project_dir(), index->location);
    }
    case SPN_INDEX_BUILTIN: {
      return sp_str_copy(index->location);
    }
  }
}
