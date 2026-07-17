#include "caps.h"

#include "enum/enum.h"
#include "triple/triple.h"

// @spader I don't know if this should literally use the same logic as we do internally to check
// stuff or if that means we will never actually catch a bug; my intent is just to say "when this
// test runs on Linux with mingw-gcc targeting Windows, I expect it to $result" for various
// combinations of facts etc.

static spn_cc_toolchain_t default_toolchain(void) {
  return (spn_cc_toolchain_t) {
    .name = sp_str_lit("zig"),
    .driver = SPN_CC_DRIVER_CLANG,
  };
}

static spn_triple_t when_target(const test_when_t* when) {
  spn_triple_t host = spn_triple_host();
  if (!when->target) {
    return host;
  }
  return spn_triple_merge(host, spn_triple_from_str(sp_str_view(when->target)));
}

sp_str_t test_when_blocked(const test_when_t* when) {
  sp_mem_t mem = sp_mem_os_new();
  spn_triple_t target = when_target(when);

  if (when->os && when->os != target.os) {
    return sp_fmt(mem, "target os is {}, test needs {}",
      sp_fmt_str(spn_os_to_str(target.os)),
      sp_fmt_str(spn_os_to_str(when->os))).value;
  }

  if (when->sanitize) {
    spn_cc_toolchain_t toolchain = default_toolchain();
    spn_profile_info_t profile = {
      .arch = target.arch,
      .os = target.os,
      .abi = target.abi,
      .linkage = SPN_LIB_KIND_SHARED,
      .sanitizers = when->sanitize,
    };
    if (spn_cc_validate_compile(&toolchain, &profile).kind) {
      return sp_fmt(mem, "{} targeting {} can't build sanitize={}",
        sp_fmt_str(toolchain.name),
        sp_fmt_str(spn_triple_to_str(mem, target)),
        sp_fmt_str(spn_sanitizer_set_to_str(mem, when->sanitize))).value;
    }
  }

  return sp_str_lit("");
}

bool test_when_runs(const test_when_t* when) {
  spn_triple_t host = spn_triple_host();
  spn_triple_t target = when_target(when);
  return target.os == host.os && target.arch == host.arch;
}
