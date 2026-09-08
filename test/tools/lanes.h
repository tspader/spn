#ifndef SPN_TEST_LANES_H
#define SPN_TEST_LANES_H

#include "sp.h"
#include "toolchains.gen.h"

#define SPN_LANES_BUILTIN "source/core/toolchain/toolchains.json"
#define SPN_LANES_TEST "test/tools/toolchains.json"

const spn_cg_toolchain_t* lanes_find(const spn_cg_toolchains_t* lanes, sp_str_t name);
const spn_cg_artifact_t*  lane_artifact(const spn_cg_toolchain_t* lane, sp_str_t host);
sp_str_t                  lanes_toml(sp_mem_t mem, const spn_cg_toolchains_t* lanes);

#endif
