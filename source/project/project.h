#ifndef SPN_PROJECT_PROJECT_H
#define SPN_PROJECT_PROJECT_H

#include "error/types.h"
#include "core/types.h"
#include "project/types.h"
#include "resolve/types.h"

sp_str_t spn_project_manifest_path(sp_mem_t mem, sp_str_t root);
spn_err_union_t spn_project_load(sp_mem_t mem, sp_intern_t* intern, spn_event_buffer_t* events, sp_str_t root, spn_project_t** project);
spn_err_union_t spn_project_update_lock(spn_project_t* project, sp_intern_t* intern, spn_resolve_t resolve);
bool spn_project_has_script(spn_project_t* project, sp_intern_t* intern, sp_str_t name);

#endif
