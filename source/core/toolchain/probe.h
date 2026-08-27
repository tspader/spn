#ifndef SPN_TOOLCHAIN_PROBE_H
#define SPN_TOOLCHAIN_PROBE_H

#include "sp.h"
#include "spn/core.h"
#include "compiler/types.h"
#include "toolchain/types.h"

sp_str_t spn_probe_env_path(sp_env_t* env);
sp_da(sp_str_t) spn_probe_split_path(sp_mem_t mem, sp_str_t path);
void spn_probe_cache_load(spn_probe_cache_t* cache, sp_str_t file, sp_mem_t mem);
spn_err_t spn_probe_cache_flush(spn_probe_cache_t* cache);
spn_err_t spn_toolchain_probe(spn_cc_toolchain_t* cc, sp_da(sp_str_t) dirs, spn_probe_cache_t* cache, sp_mem_t mem, sp_hash_t* identity);

#endif
