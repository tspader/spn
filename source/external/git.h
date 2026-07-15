#ifndef SPN_EXTERNAL_GIT_H
#define SPN_EXTERNAL_GIT_H

#include "sp.h"
#include "error/types.h"

spn_err_t spn_git_clone(sp_str_t url, sp_str_t path);
spn_err_t spn_git_fetch(sp_str_t repo);
u32 spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to);
spn_err_t spn_git_checkout(sp_str_t repo, sp_str_t commit);
spn_err_t spn_git_get_remote_url(sp_mem_t mem, sp_str_t repo_path, sp_str_t* url);
spn_err_t spn_git_get_commit(sp_mem_t mem, sp_str_t repo_path, sp_str_t id, sp_str_t* sha);
spn_err_t spn_git_get_commit_full(sp_mem_t mem, sp_str_t repo_path, sp_str_t id, sp_str_t* sha);
sp_str_t spn_git_get_commit_message(sp_mem_t mem, sp_str_t repo_path, sp_str_t id);
spn_err_t spn_git_get_root(sp_mem_t mem, sp_str_t cwd, sp_str_t* root);
spn_err_t spn_git_default_branch(sp_mem_t mem, sp_str_t repo, sp_str_t* branch);
spn_err_t spn_git_current_branch(sp_mem_t mem, sp_str_t repo, sp_str_t* branch);
bool spn_git_has_remote_branches(sp_str_t repo);
bool spn_git_is_repo_root(sp_str_t repo);
spn_err_t spn_git_checkout_branch(sp_str_t repo, sp_str_t branch);
spn_err_t spn_git_clean(sp_str_t repo);
spn_err_t spn_git_add(sp_str_t repo, sp_str_t path);
spn_err_t spn_git_commit(sp_str_t repo, sp_str_t message);
spn_err_t spn_git_push(sp_mem_t mem, sp_str_t repo, sp_str_t url, sp_str_t refspec, sp_str_t* output);
bool spn_git_is_dirty(sp_str_t repo, sp_str_t dir);
bool spn_git_rev_on_remote(sp_str_t repo, sp_str_t rev);

#endif
