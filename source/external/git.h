#ifndef SPN_EXTERNAL_GIT_H
#define SPN_EXTERNAL_GIT_H

#include "sp.h"
#include "spn.h"

spn_err_t spn_git_clone(sp_str_t url, sp_str_t path);
spn_err_t spn_git_fetch(sp_str_t repo);
u32 spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to);
spn_err_t spn_git_checkout(sp_str_t repo, sp_str_t commit);
sp_str_t spn_git_get_remote_url(sp_str_t repo_path);
sp_str_t spn_git_get_commit(sp_str_t repo_path, sp_str_t id);
sp_str_t spn_git_get_commit_message(sp_str_t repo_path, sp_str_t id);

#endif
