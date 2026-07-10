#ifndef SPN_PUBLISH_H
#define SPN_PUBLISH_H

#include "sp.h"

#include "error/types.h"
#include "forward/types.h"
#include "index/types.h"
#include "intern/types.h"

typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  sp_str_t cwd;
  sp_str_t url;
  sp_str_t revision;
  bool allow_dirty;
} spn_publish_opts_t;

spn_err_union_t spn_publish_build(spn_publish_opts_t* opts, spn_index_release_t* out);

#endif
