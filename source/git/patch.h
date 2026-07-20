#ifndef SPN_GIT_PATCH_H
#define SPN_GIT_PATCH_H

#include "sp.h"

#include "git/types.h"

spn_err_t spn_git_patch_set_load(sp_mem_t mem, sp_da(sp_str_t) files, spn_git_patch_set_t* out, sp_str_t* missing);

#endif
