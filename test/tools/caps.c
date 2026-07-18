#include "caps.h"

#include "enum/enum.h"
#include "triple/triple.h"

// @spader I don't know if this should literally use the same logic as we do internally to check
// stuff or if that means we will never actually catch a bug; my intent is just to say "when this
// test runs on Linux with mingw-gcc targeting Windows, I expect it to $result" for various
// combinations of facts etc.

static const test_toolchain_t toolchains [] = {
  {
    .name = "zig",
    .driver = SPN_CC_DRIVER_CLANG,
    .targets = {
      "wasm32-wasi-musl",
      "x86_64-linux-gnu",
      "aarch64-linux-gnu",
      "x86_64-linux-musl",
      "aarch64-linux-musl",
      "x86_64-macos",
      "aarch64-macos",
      "x86_64-windows-gnu",
      "aarch64-windows-gnu",
    },
  },
  {
    .name = "msvc",
    .driver = SPN_CC_DRIVER_MSVC,
    .abi = SPN_ABI_MSVC,
    .targets = {
      "x86_64-windows-msvc",
      "aarch64-windows-msvc",
    },
  },
  {
    .name = "system",
    .driver = SPN_CC_DRIVER_GCC,
  },
};

const test_toolchain_t* test_toolchain(void) {
  static const test_toolchain_t* cached = SP_NULLPTR;
  if (cached) {
    return cached;
  }

  sp_str_t name = sp_os_env_get(sp_str_lit("SPN_TEST_TOOLCHAIN"));
  if (sp_str_empty(name)) {
    name = sp_str_lit("zig");
  }

  sp_carr_for(toolchains, it) {
    if (sp_str_equal_cstr(name, toolchains[it].name)) {
      cached = &toolchains[it];
      return cached;
    }
  }

  SP_ASSERT(cached);
  return SP_NULLPTR;
}

spn_triple_t test_host(void) {
  spn_triple_t host = spn_triple_host();
  const test_toolchain_t* toolchain = test_toolchain();
  if (toolchain->abi && host.os == SPN_OS_WINDOWS) {
    host.abi = toolchain->abi;
  }
  return host;
}

static spn_triple_t when_target(const test_when_t* when) {
  spn_triple_t host = test_host();
  if (!when->target) {
    return host;
  }
  return spn_triple_merge(host, spn_triple_from_str(sp_str_view(when->target)));
}

static bool toolchain_targets(const test_toolchain_t* toolchain, spn_triple_t target) {
  if (!toolchain->targets[0]) {
    spn_triple_t host = spn_triple_host();
    return target.os == host.os && target.arch == host.arch;
  }

  sp_carr_for(toolchain->targets, it) {
    if (!toolchain->targets[it]) {
      break;
    }
    if (spn_triple_match(spn_triple_from_str(sp_str_view(toolchain->targets[it])), target)) {
      return true;
    }
  }

  return false;
}

const c8* test_target_alternate(void) {
  const test_toolchain_t* toolchain = test_toolchain();
  spn_triple_t host = spn_triple_host();

  sp_carr_for(toolchain->targets, it) {
    if (!toolchain->targets[it]) {
      break;
    }
    spn_triple_t target = spn_triple_from_str(sp_str_view(toolchain->targets[it]));
    if (target.os != host.os || target.arch != host.arch) {
      return toolchain->targets[it];
    }
  }

  return SP_NULLPTR;
}

sp_str_t test_when_blocked(const test_when_t* when) {
  sp_mem_t mem = sp_mem_os_new();
  const test_toolchain_t* toolchain = test_toolchain();
  spn_triple_t target = when_target(when);

  if (when->os && when->os != target.os) {
    return sp_fmt(mem, "target os is {}, test needs {}",
      sp_fmt_str(spn_os_to_str(target.os)),
      sp_fmt_str(spn_os_to_str(when->os))).value;
  }

  if (!toolchain_targets(toolchain, target)) {
    return sp_fmt(mem, "{} can't target {}",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target))).value;
  }

  if (when->sanitize) {
    spn_cc_toolchain_t cc = {
      .name = sp_str_view(toolchain->name),
      .driver = toolchain->driver,
    };
    spn_profile_info_t profile = {
      .arch = target.arch,
      .os = target.os,
      .abi = target.abi,
      .linkage = SPN_LIB_KIND_SHARED,
      .sanitizers = when->sanitize,
    };
    if (spn_cc_validate_compile(&cc, &profile).kind) {
      return sp_fmt(mem, "{} targeting {} can't build sanitize={}",
        sp_fmt_cstr(toolchain->name),
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
