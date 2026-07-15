#include "spn.h"
#include "abi.gen.h"

__attribute__((export_name("configure")))
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_run_ex(spn, (spn_run_t) {
    .target = spn_get_target(spn, "spn"),
    .args = { "tool/mkopcodeh.tcl", "parse.h", "src/vdbe.c" },
    .std = { .out = "opcodes.h" },
  });

  spn_target_t* target = spn_get_target(spn, "spn");
  spn_target_embed_file_ex(target, "include/spn.h", "include_spn_h", "u8", "u64");
  spn_target_embed_file_ex(target, "source/toolchain/toolchains.json", "toolchains_json", "u8", "u64");
  spn_target_embed_dir_ex(target, "assets/init", "init", "u8", "u64");
  return SPN_OK;
}
