#ifndef SPN_GIT_CACHE_H
#define SPN_GIT_CACHE_H

#include "git/types.h"
#include "error/types.h"

void      spn_git_cache_init(spn_git_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, sp_str_t root);
spn_err_t spn_git_cache_ensure_db(spn_git_cache_t* cache, sp_str_t url, spn_git_db_t** db);
spn_err_t spn_git_cache_ensure_checkout(spn_git_cache_t* cache, spn_git_checkout_id_t id, spn_git_checkout_t** checkout);
spn_err_t spn_git_db_ensure_rev(spn_git_db_t* db, sp_str_t rev);

#endif
