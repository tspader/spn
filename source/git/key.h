#ifndef SPN_GIT_KEY_H
#define SPN_GIT_KEY_H

#include "sp.h"

sp_str_t spn_git_db_key(sp_str_t url);
sp_str_t spn_git_checkout_key(sp_str_t url, sp_str_t rev, sp_str_t dir);
sp_str_t spn_git_url_name(sp_str_t url);

#endif
