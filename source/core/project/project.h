#ifndef SPN_PROJECT_PROJECT_H
#define SPN_PROJECT_PROJECT_H

#include "ctx/types.h"
#include "sp.h"
#include "spn/core.h"
#include "core/types.h"
#include "project/types.h"
#include "resolve/types.h"

sp_str_t spn_project_manifest_path(sp_mem_t mem, sp_str_t root);
spn_err_t spn_project_load(spn_ctx_t* ctx, sp_str_t root, spn_project_t** project);
spn_err_t spn_project_update_lock(spn_ctx_t* ctx, spn_project_t* project, spn_resolve_t resolve);

#endif
