#ifndef SPN_INDEX_H
#define SPN_INDEX_H

#include "sp.h"
#include "spn.h"

#include "pkg.h"

typedef enum {
  SPN_INDEX_WORKSPACE,
  SPN_INDEX_BUILTIN,
} spn_index_kind_t;

struct spn_index {
  sp_str_t name;
  sp_str_t location;
  spn_index_kind_t kind;
};

sp_str_t spn_index_get_path(spn_index_t* index);

#endif
