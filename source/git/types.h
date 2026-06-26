#ifndef SPN_GIT_TYPES_H
#define SPN_GIT_TYPES_H

#include "sp.h"
#include "sp/sp_om.h"

typedef struct {
  sp_str_t url;
  sp_str_t path;
} spn_git_db_t;

typedef struct {
  sp_str_t url;
  sp_str_t rev;
  sp_str_t dir;
} spn_git_checkout_id_t;

typedef struct {
  spn_git_checkout_id_t id;
  sp_str_t path;
} spn_git_checkout_t;

typedef struct {
  sp_str_t root;
  struct {
    sp_str_t dir;
    sp_str_ht(spn_git_db_t) entries;
  } db;
  struct {
    sp_str_t dir;
    sp_str_om(spn_git_checkout_t) entries;
  } checkouts;
} spn_git_cache_t;

#endif
