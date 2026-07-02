#include "spn.h"
#include "abi.gen.h"

__attribute__((export_name("configure")))
spn_err_t configure(spn_t* spn) {
  spn_run_ex(spn, (spn_run_t) {
    .target = spn_get_target(spn, "spn"),
    .args = { "tool/mkopcodeh.tcl", "parse.h", "src/vdbe.c" },
    .std = { .out = "opcodes.h" },
  });
  return SPN_OK;
}
