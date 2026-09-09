#ifndef SPN_TEST_LANES_H
#define SPN_TEST_LANES_H

#include "sp.h"
#include "codegen/types.h"
#include "config.gen.h"
#include "toolchain/types.h"

#define SPN_LANES_BUILTIN "source/core/toolchain/toolchains.toml"
#define SPN_LANES_TEST "test/tools/lanes.toml"

// A [[toolchain]] file as both consumers of the lanes see it: the parsed
// entries, and the text they came from so one entry can be handed to spn
// verbatim. Entries are lowered one at a time so an entry that spn refuses
// is one red lane rather than a broken file.
typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  sp_str_t path;
  sp_str_t text;
  spn_cg_config_t config;
  sp_da(spn_codegen_issue_t) issues;
} lanes_t;

bool                           lanes_read(sp_mem_t mem, sp_str_t path, lanes_t* out, sp_str_t* issues);
const spn_cg_toolchain_decl_t* lanes_find(const lanes_t* lanes, sp_str_t name);
sp_str_t                       lanes_lower(const lanes_t* lanes, u32 at, spn_path_root_t base, spn_toolchain_decl_t* out);
sp_str_t                       lanes_text(const lanes_t* lanes, sp_str_t name);
const spn_cg_artifact_t*       lane_artifact(const spn_cg_toolchain_decl_t* lane, sp_str_t host);

#endif
