#include "caps.h"

#include "enum/enum.h"
#include "triple/triple.h"

static const test_toolchain_t toolchains [] = {
  {
    .name = "zig",
    .driver = SPN_CC_DRIVER_ZIG,
    .abi = SPN_ABI_GNU,
    .targets = {
      "wasm32-wasi-musl",
      "x86_64-linux-gnu",
      "aarch64-linux-gnu",
      "x86_64-linux-musl",
      "aarch64-linux-musl",
      "x86_64-macos-apple",
      "aarch64-macos-apple",
      "x86_64-windows-gnu",
      "aarch64-windows-gnu",
      "x86_64-freestanding-none",
      "aarch64-freestanding-none",
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
    .name = "clang",
    .driver = SPN_CC_DRIVER_CLANG,
    .targets = { SPN_TEST_HOST_TARGETS },
  },
  {
    .name = "gcc",
    .driver = SPN_CC_DRIVER_GCC,
    .targets = { SPN_TEST_HOST_TARGETS },
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

static spn_triple_t parse_triple(const c8* str) {
  spn_triple_t triple = sp_zero;
  sp_assert(spn_triple_parse(sp_cstr_as_str(str), &triple) == SPN_OK);
  return triple;
}

static spn_triple_t when_target(const test_when_t* when) {
  spn_triple_t host = test_host();
  if (!when->target) {
    return host;
  }
  spn_triple_t partial = parse_triple(when->target);
  return (spn_triple_t) {
    .arch = partial.arch ? partial.arch : host.arch,
    .os = partial.os ? partial.os : host.os,
    .abi = partial.abi,
  };
}

static bool triple_agrees(spn_triple_t a, spn_triple_t b) {
  bool arch = !a.arch || !b.arch || a.arch == b.arch;
  bool os = !a.os || !b.os || a.os == b.os;
  bool abi = !a.abi || !b.abi || a.abi == b.abi;
  return arch && os && abi;
}

static bool toolchain_targets(const test_toolchain_t* toolchain, spn_triple_t target) {
  sp_carr_for(toolchain->targets, it) {
    if (!toolchain->targets[it]) {
      break;
    }
    if (triple_agrees(parse_triple(toolchain->targets[it]), target)) {
      return true;
    }
  }
  return false;
}

const c8* test_target_alternate(void) {
  const test_toolchain_t* toolchain = test_toolchain();
  spn_triple_t host = test_host();

  sp_carr_for(toolchain->targets, it) {
    if (!toolchain->targets[it]) {
      break;
    }
    spn_triple_t target = parse_triple(toolchain->targets[it]);
    if (target.os == SPN_OS_FREESTANDING) {
      continue;
    }
    if (target.os != host.os || target.arch != host.arch) {
      return toolchain->targets[it];
    }
  }

  return SP_NULLPTR;
}

spn_sanitizer_set_t get_supported_sanitizers(const spn_cc_toolchain_t* toolchain, spn_triple_t target);

static bool toolchain_enforces_exports(const test_toolchain_t* toolchain, spn_triple_t target) {
  if (sp_cstr_equal(toolchain->name, "zig") && target.os == SPN_OS_MACOS) {
    return false;
  }
  return true;
}

static bool toolchain_deterministic_objects(const test_toolchain_t* toolchain) {
  return toolchain->driver != SPN_CC_DRIVER_MSVC;
}

sp_str_t test_when_blocked(test_when_t when) {
  sp_mem_t mem = sp_mem_os_new();
  const test_toolchain_t* toolchain = test_toolchain();
  spn_triple_t target = when_target(&when);

  if (when.os && when.os != target.os) {
    return sp_fmt(mem, "target os is {}, test needs {}",
      sp_fmt_str(spn_os_to_str(target.os)),
      sp_fmt_str(spn_os_to_str(when.os))).value;
  }

  if (!toolchain_targets(toolchain, target)) {
    return sp_fmt(mem, "{} can't target {}",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target))).value;
  }

  if (when.sanitize) {
    spn_cc_toolchain_t cc = {
      .name = sp_str_view(toolchain->name),
      .driver = toolchain->driver,
    };
    if (when.sanitize & ~get_supported_sanitizers(&cc, target)) {
      return sp_fmt(mem, "{} targeting {} can't build sanitize={}",
        sp_fmt_cstr(toolchain->name),
        sp_fmt_str(spn_triple_to_str(mem, target)),
        sp_fmt_str(spn_sanitizer_set_to_str(mem, when.sanitize))).value;
    }
  }

  if (when.exports && !toolchain_enforces_exports(toolchain, target)) {
    return sp_fmt(mem, "{} targeting {} accepts an export list but does not enforce it",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target))).value;
  }

  if (when.deterministic && !toolchain_deterministic_objects(toolchain)) {
    return sp_fmt(mem, "{} does not recompile objects byte-identically",
      sp_fmt_cstr(toolchain->name)).value;
  }

  if (when.msvc_todo && toolchain->driver == SPN_CC_DRIVER_MSVC) {
    return sp_str_lit("not yet implemented for the msvc toolchain");
  }

  return sp_str_lit("");
}

bool test_when_runs(const test_when_t* when) {
  spn_triple_t host = spn_triple_host();
  spn_triple_t target = when_target(when);
  return target.os == host.os && target.arch == host.arch;
}
