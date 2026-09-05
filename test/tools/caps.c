#include "caps.h"
#include "fixture.h"

#include "enum/enum.h"
#include "toolchain/catalog.h"
#include "toolchain/linker.h"
#include "toolchain/search.h"
#include "triple/triple.h"

static test_toolchain_t cached;

const test_toolchain_t* test_toolchain(void) {
  if (cached.info) {
    return &cached;
  }

  sp_mem_t mem = sp_mem_os_new();
  sp_str_t name = sp_os_env_get(sp_str_lit("SPN_TEST_TOOLCHAIN"));
  if (sp_str_empty(name)) {
    name = sp_str_lit("zig");
  }

  sp_str_t json = sp_zero;
  SP_ASSERT(!sp_io_read_file(mem, test_repo_path(mem, sp_str_lit("source/core/toolchain/toolchains.json")), &json));
  spn_toolchain_catalog_t* catalog = sp_alloc_type(mem, spn_toolchain_catalog_t);
  spn_toolchain_catalog_init(catalog, spn_triple_host(), mem);
  SP_ASSERT(spn_toolchain_catalog_load(catalog, json) == SPN_OK);

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(catalog, name);
  SP_ASSERT(info);
  cached = (test_toolchain_t) { .name = sp_str_to_cstr(mem, info->name), .info = info };
  return &cached;
}

static bool targets(const spn_toolchain_info_t* info, spn_triple_t triple) {
  sp_da_for(info->targets, it) {
    if (spn_triple_equal(info->targets[it], triple)) {
      return true;
    }
  }
  return false;
}

spn_triple_t test_host(void) {
  spn_triple_t host = spn_triple_host();
  const spn_toolchain_info_t* info = test_toolchain()->info;
  if (targets(info, host)) {
    return host;
  }
  sp_da_for(info->targets, it) {
    if (info->targets[it].arch == host.arch && info->targets[it].os == host.os) {
      return info->targets[it];
    }
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

static bool toolchain_targets(const spn_toolchain_info_t* info, spn_triple_t target) {
  sp_da_for(info->targets, it) {
    if (triple_agrees(info->targets[it], target)) {
      return true;
    }
  }
  return false;
}

const c8* test_target_alternate(void) {
  const spn_toolchain_info_t* info = test_toolchain()->info;
  spn_triple_t host = test_host();

  sp_da_for(info->targets, it) {
    spn_triple_t target = info->targets[it];
    if (target.os == SPN_OS_FREESTANDING) {
      continue;
    }
    if (target.os != host.os || target.arch != host.arch) {
      sp_mem_t mem = sp_mem_os_new();
      return sp_str_to_cstr(mem, spn_triple_to_str(mem, target));
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
  return toolchain->info->driver != SPN_CC_DRIVER_MSVC;
}

static bool installed(sp_mem_t mem, sp_str_t program) {
  sp_da(sp_str_t) dirs = spn_search_split_path(mem, sp_os_env_get(sp_str_lit("PATH")));
  return !sp_str_empty(spn_search_program(mem, sp_fs_get_cwd(mem), program, dirs));
}

static sp_str_t missing_toolchain_program(sp_mem_t mem, const spn_toolchain_info_t* info, spn_triple_t target) {
  sp_str_t programs [] = {
    info->compiler.program.prefix,
    info->archiver.program.prefix,
    info->linkers.slots[spn_ld_flavor(target)].program.prefix,
  };
  sp_carr_for(programs, it) {
    if (!sp_str_empty(programs[it]) && !installed(mem, programs[it])) {
      return programs[it];
    }
  }
  return sp_str_lit("");
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

  spn_triple_t host = spn_triple_host();
  if (when.host && when.host != host.os) {
    return sp_fmt(mem, "host os is {}, test needs {}",
      sp_fmt_str(spn_os_to_str(host.os)),
      sp_fmt_str(spn_os_to_str(when.host))).value;
  }

  if (when.shell && host.os == SPN_OS_WINDOWS) {
    return sp_str_lit("fixture needs a posix shell");
  }

  sp_carr_for(when.programs, it) {
    if (!when.programs[it]) {
      break;
    }
    if (!installed(mem, sp_cstr_as_str(when.programs[it]))) {
      return sp_fmt(mem, "{} isn't installed", sp_fmt_cstr(when.programs[it])).value;
    }
  }

  if (when.driver && when.driver != toolchain->info->driver) {
    return sp_fmt(mem, "{} isn't a {} driver",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_cc_driver_to_str(when.driver))).value;
  }

  if (when.toolchain) {
    return sp_str_lit("");
  }

  if (!toolchain_targets(toolchain->info, target)) {
    return sp_fmt(mem, "{} can't target {}",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target))).value;
  }

  sp_str_t missing = toolchain->info->support.kind == SPN_TOOLCHAIN_SUPPORT_LOCAL ? missing_toolchain_program(mem, toolchain->info, target) : sp_str_lit("");
  if (!sp_str_empty(missing)) {
    return sp_fmt(mem, "{} needs {}, which isn't installed", sp_fmt_cstr(toolchain->name), sp_fmt_str(missing)).value;
  }

  if (when.sanitize) {
    spn_cc_toolchain_t cc = {
      .name = toolchain->info->name,
      .driver = toolchain->info->driver,
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

  if (when.msvc_todo && toolchain->info->driver == SPN_CC_DRIVER_MSVC) {
    return sp_str_lit("not yet implemented for the msvc toolchain");
  }

  return sp_str_lit("");
}

bool test_when_runs(const test_when_t* when) {
  spn_triple_t host = spn_triple_host();
  spn_triple_t target = when_target(when);
  return target.os == host.os && target.arch == host.arch;
}
