#include "registry.h"

#include "ctx.h"

sp_str_t spn_registry_get_path(spn_registry_t* registry) {
  switch (registry->kind) {
    case SPN_PACKAGE_KIND_WORKSPACE: {
      return sp_fs_join_path(spn_app_project_dir(), registry->location);
    }
    case SPN_PACKAGE_KIND_INDEX: {
      return sp_str_copy(registry->location);
    }
    case SPN_PACKAGE_KIND_ROOT:
    case SPN_PACKAGE_KIND_FILE:
    case SPN_PACKAGE_KIND_NONE: {
      SP_UNREACHABLE();
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}
