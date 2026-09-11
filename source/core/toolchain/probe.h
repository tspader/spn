#ifndef SPN_TOOLCHAIN_PROBE_H
#define SPN_TOOLCHAIN_PROBE_H

#include "sp.h"
#include "spn/core.h"
#include "compiler/types.h"
#include "paths/types.h"
#include "toolchain/search.h"
#include "toolchain/types.h"

void spn_probe_cache_load(spn_probe_cache_t* cache, sp_str_t file, sp_mem_t mem);
spn_err_t spn_probe_cache_flush(spn_probe_cache_t* cache);
spn_err_t spn_toolchain_probe(spn_cc_toolchain_t* cc, const spn_path_roots_t* roots, spn_search_rules_t rules, sp_str_t path, spn_probe_cache_t* cache, sp_mem_t mem, sp_hash_t* identity);

#endif
