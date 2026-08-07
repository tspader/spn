#ifndef SPN_PROJECT_SCAFFOLD_H
#define SPN_PROJECT_SCAFFOLD_H

#include "sp.h"

#include "error/types.h"
#include "project/types.h"

spn_err_union_t spn_project_scaffold(sp_mem_t mem, spn_scaffold_desc_t desc, sp_da(sp_str_t)* files);

#endif
