#ifndef SPN_GIT_TYPES_H
#define SPN_GIT_TYPES_H

#include "sp.h"
#include "sp/sp_om.h"

#include "intern/types.h"
#include "spn.h"

typedef struct {
  sp_str_t url;
  sp_str_t path;
  sp_mutex_t mutex;
  bool ready;
  spn_err_t err;
  sp_str_t error;
} spn_git_db_t;

// hash is the set's identity: derived from file contents in order, zero iff
// files is empty. Checkout keys and fingerprints treat zero as unpatched.
typedef struct {
  sp_da(sp_str_t) files;
  sp_hash_t hash;
} spn_git_patch_set_t;

typedef struct {
  sp_str_t url;
  sp_str_t rev;
  sp_str_t dir;
  spn_git_patch_set_t patches;
} spn_git_checkout_id_t;

typedef struct {
  spn_git_checkout_id_t id;
  sp_str_t path;
  sp_mutex_t mutex;
  bool ready;
  bool fetched;
  spn_err_t err;
  sp_str_t error;
} spn_git_checkout_t;

typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  sp_str_t root;
  sp_mutex_t mutex;
  struct {
    sp_str_t dir;
    sp_str_ht(spn_git_db_t*) entries;
  } db;
  struct {
    sp_str_t dir;
    sp_str_om(spn_git_checkout_t*) entries;
  } checkouts;
} spn_git_cache_t;

#endif
