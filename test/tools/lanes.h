#ifndef SPN_TEST_LANES_H
#define SPN_TEST_LANES_H

#include "sp.h"
#include "codegen/types.h"
#include "config.gen.h"
#include "toolchain/types.h"

#define SPN_LANES_BUILTIN "source/core/toolchain/toolchains.toml"
#define SPN_LANES_TEST "test/tools/lanes.toml"

typedef enum {
  LANES_READ_OK,
  LANES_READ_UNREADABLE,
  LANES_READ_PARSE,
} lanes_read_t;

typedef struct {
  sp_mem_t mem;
  sp_intern_t* intern;
  sp_str_t path;
  sp_str_t text;
  spn_cg_config_t config;
  sp_da(spn_codegen_issue_t) issues;
} lanes_t;

lanes_read_t                   lanes_read(sp_mem_t mem, sp_str_t path, lanes_t* lanes);
const spn_cg_toolchain_decl_t* lanes_find(const lanes_t* lanes, sp_str_t name);
sp_da(spn_codegen_issue_t)     lanes_lower(const lanes_t* lanes, u32 at, spn_path_root_t base, spn_toolchain_decl_t* decl);
sp_str_t                       lanes_text(const lanes_t* lanes, sp_str_t name);
const spn_cg_artifact_t*       lane_artifact(const spn_cg_toolchain_decl_t* lane, sp_str_t host);

#endif
