#ifndef SPN_PUBLISH_H
#define SPN_PUBLISH_H

#include "sp.h"

#include "error/types.h"
#include "forward/types.h"
#include "intern/types.h"

typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  sp_str_t cwd;
  spn_index_info_t* index;
  sp_str_t url;
  sp_str_t revision;
} spn_publish_opts_t;

spn_err_union_t spn_publish(spn_publish_opts_t* opts);

#endif
