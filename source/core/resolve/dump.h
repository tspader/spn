#ifndef SPN_RESOLVE_DUMP_H
#define SPN_RESOLVE_DUMP_H

#include "sp.h"

#include "intern/types.h"
#include "resolve/types.h"
#include "resolve.gen.h"

#define SPN_RESOLVE_DUMP_VERSION 2

spn_cg_resolve_t spn_resolve_dump(sp_mem_t mem, sp_intern_t* intern, const spn_resolve_query_t* query);

#endif
