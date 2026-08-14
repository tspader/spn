#ifndef SPN_GIT_KEY_H
#define SPN_GIT_KEY_H

#include "sp.h"

#include "git/types.h"

sp_str_t spn_git_db_key(sp_mem_t mem, sp_str_t url);
sp_str_t spn_git_checkout_key(sp_mem_t mem, spn_git_checkout_id_t id);
sp_str_t spn_git_url_name(sp_str_t url);

#endif
