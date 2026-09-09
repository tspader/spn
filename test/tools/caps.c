#include "caps.h"
#include "sp/sp_test.h"
#include "fixture.h"

#include "enum/enum.h"
#include "ctx/types.h"
#include "event/event.h"
#include "profile/profile.h"
#include "paths/paths.h"
#include "toolchain/catalog.h"
#include "toolchain/linker.h"
#include "toolchain/search.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "lanes.h"

static sp_test_once_t once;
static test_toolchain_t cached;
static spn_toolchain_catalog_t catalog;
static sp_str_t toml;

static sp_str_t read_repo_file(sp_mem_t mem, const c8* rel) {
  sp_str_t content = sp_zero;
  sp_assert(!sp_io_read_file(mem, test_repo_path(mem, sp_cstr_as_str(rel)), &content));
  return content;
}

static bool targets(const spn_toolchain_info_t* info, spn_triple_t triple) {
  sp_da_for(info->rows, it) {
    if (spn_triple_equal(info->rows[it].triple, triple)) {
      return true;
    }
  }
  return false;
}

static spn_triple_t host_for(const spn_toolchain_info_t* info) {
  spn_triple_t host = spn_triple_host();
  if (targets(info, host)) {
    return host;
  }
  sp_da_for(info->rows, it) {
    if (info->rows[it].triple.arch == host.arch && info->rows[it].triple.os == host.os) {
      return info->rows[it].triple;
    }
  }
  return host;
}

spn_triple_t test_host(void) {
  return host_for(test_toolchain()->info);
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

const c8* test_host_triple(void) {
  sp_mem_t mem = sp_mem_os_new();
  return sp_str_to_cstr(mem, spn_triple_to_str(mem, test_host()));
}

const c8* test_target_alternate(void) {
  const spn_toolchain_info_t* info = test_toolchain()->info;
  spn_triple_t host = test_host();

  sp_da_for(info->rows, it) {
    spn_triple_t target = info->rows[it].triple;
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
  return !sp_str_empty(spn_search_program(mem, program, dirs));
}

static bool present(sp_mem_t mem, spn_arg_t program) {
  if (!sp_str_empty(program.prefix)) {
    return installed(mem, program.prefix);
  }
  return sp_fs_is_target_file(program.path.sub);
}

typedef struct {
  const c8* lane;
  spn_ld_dialect_t dialect;
  const c8* program;
} lane_program_t;

static const lane_program_t lane_programs [] = {
  { "llvm",            SPN_LD_DIALECT_GNU,    "ld.lld" },
  { "llvm",            SPN_LD_DIALECT_DARWIN, "ld64.lld" },
  { "gcc-lld",         SPN_LD_DIALECT_GNU,    "ld.lld" },
  { "clang-msvc",      SPN_LD_DIALECT_LINK,   "lld-link" },
  { "clang-xwin",      SPN_LD_DIALECT_LINK,   "lld-link-16" },
  { "clang-mingw",     SPN_LD_DIALECT_GNU,    "x86_64-w64-mingw32-ld" },
  { "clang-mingw-lld", SPN_LD_DIALECT_GNU,    "ld.lld" },
  { "clang-sysroot",   SPN_LD_DIALECT_GNU,    "ld.lld" },
  { "clang-wasi",      SPN_LD_DIALECT_WASM,   "wasm-ld" },
};

static bool links_dialect(const spn_toolchain_info_t* info, spn_ld_dialect_t dialect) {
  sp_da_for(info->rows, it) {
    if (spn_ld_dialect(info->rows[it].triple) == dialect) {
      return true;
    }
  }
  return false;
}

static sp_str_t missing_lane_program(sp_mem_t mem, const spn_toolchain_info_t* info) {
  sp_carr_for(lane_programs, it) {
    const lane_program_t* row = &lane_programs[it];
    if (!sp_str_equal_cstr(info->name, row->lane) || !links_dialect(info, row->dialect)) {
      continue;
    }
    if (!installed(mem, sp_cstr_as_str(row->program))) {
      return sp_cstr_as_str(row->program);
    }
  }
  return sp_str_lit("");
}

static sp_str_t missing_toolchain_program(sp_mem_t mem, const spn_toolchain_info_t* info) {
  spn_arg_t programs [] = {
    info->compiler.program,
    info->archiver.program,
  };
  sp_carr_for(programs, it) {
    if (!present(mem, programs[it])) {
      return sp_str_empty(programs[it].prefix) ? programs[it].path.sub : programs[it].prefix;
    }
  }
  return missing_lane_program(mem, info);
}

static sp_str_t lane_broken(sp_mem_t mem, const spn_toolchain_info_t* info) {
  switch (info->support.kind) {
    case SPN_TOOLCHAIN_SUPPORT_NONE: {
      return sp_fmt(mem, "doesn't support {}", sp_fmt_str(spn_triple_to_str(mem, spn_triple_host()))).value;
    }
    case SPN_TOOLCHAIN_SUPPORT_ARTIFACT: {
      return sp_str_lit("");
    }
    case SPN_TOOLCHAIN_SUPPORT_LOCAL: {
      sp_str_t missing = missing_toolchain_program(mem, info);
      if (!sp_str_empty(missing)) {
        return sp_fmt(mem, "{} isn't installed", sp_fmt_str(missing)).value;
      }
      return sp_str_lit("");
    }
  }
  sp_unreachable_return(sp_str_lit(""));
}

static sp_err_t load_lanes(void* user) {
  sp_mem_t mem = sp_mem_os_new();
  sp_str_t name = sp_os_env_get(sp_str_lit("SPN_TEST_TOOLCHAIN"));
  if (sp_str_empty(name)) {
    name = sp_str_lit("zig");
  }

  spn.mem = mem;
  spn.events = spn_event_buffer_new(mem);
  sp_str_t lanes = read_repo_file(mem, SPN_LANES_TEST);
  sp_env_t env = sp_env_capture(mem);
  spn_path_roots_t roots = sp_zero;
  spn_toolchain_catalog_init(&catalog, spn_triple_host(), spn_sdk_detect(mem, &roots, &env, spn_triple_host()), mem);
  sp_assert(spn_toolchain_catalog_load(&catalog, read_repo_file(mem, SPN_LANES_BUILTIN)) == SPN_OK);
  sp_assert(spn_toolchain_catalog_load(&catalog, lanes) == SPN_OK);

  spn_cg_toolchains_t parsed = sp_zero;
  sp_assert(spn_toolchains_read(lanes, &parsed, mem));
  toml = lanes_toml(mem, &catalog, &parsed);

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, name);
  if (!info) {
    sp_log("unknown lane {.red}", sp_fmt_str(name));
    sp_sys_exit(1);
  }
  sp_str_t broken = lane_broken(mem, info);
  if (!sp_str_empty(broken)) {
    sp_log("lane {.red} is broken: {}", sp_fmt_str(name), sp_fmt_str(broken));
    sp_sys_exit(1);
  }
  cached = (test_toolchain_t) { .name = sp_str_to_cstr(mem, info->name), .info = info };
  return SP_OK;
}

const test_toolchain_t* test_toolchain(void) {
  sp_test_once(&once, load_lanes, SP_NULLPTR);
  return &cached;
}

const c8* test_lane_toolchain_arg(void) {
  const test_toolchain_t* toolchain = test_toolchain();
  return sp_cstr_equal(toolchain->name, "zig") ? SP_NULLPTR : toolchain->name;
}

static const c8* select_reason(spn_err_t err) {
  switch (err) {
    case SPN_ERR_TOOLCHAIN_NONE: return "no toolchain can";
    case SPN_ERR_TOOLCHAIN_HOST: return "doesn't run on this host";
    case SPN_ERR_TOOLCHAIN_TARGET: return "doesn't target it";
    case SPN_ERR_TOOLCHAIN_SYSROOT: return "needs a sysroot";
    case SPN_ERR_TOOLCHAIN_SDK_MACOS: return "needs the macOS SDK";
    case SPN_ERR_TOOLCHAIN_SDK_MSVC: return "needs the MSVC SDK";
    case SPN_ERR_TARGET_ABI: return "needs an abi";
    case SPN_ERR_SANITIZER_UNSUPPORTED: return "doesn't ship those sanitizers";
    case SPN_ERR_SANITIZER_STATIC: return "links it statically";
    default: return "can't select it";
  }
}

static spn_err_t lane_selects(sp_mem_t mem, const test_when_t* when, spn_triple_t target, spn_toolchain_selection_t* selection) {
  const c8* named = test_lane_toolchain_arg();
  spn_profile_info_t profile = {
    .toolchain = spn_toolchain_ref_from_str(sp_cstr_as_str(named ? named : "auto")),
    .arch = target.arch,
    .os = target.os,
    .abi = when->target ? target.abi : SPN_ABI_NONE,
    .sanitizers = when->sanitize,
  };
  spn_toolchain_query_t query = spn_profile_query(&profile, spn_triple_host());
  spn_err_t err = query.abis.count ? spn_toolchain_select(&catalog, query, selection) : spn_toolchain_incomplete(&catalog, query);
  spn_event_buffer_drain(mem, spn.events);
  return err;
}

static sp_str_t not_in_lanes(sp_mem_t mem, const test_toolchain_t* toolchain, const c8* const* lanes, u32 count) {
  sp_for(it, count) {
    if (!spn_toolchain_catalog_get(&catalog, sp_cstr_as_str(lanes[it]))) {
      sp_log("unknown lane {.red}", sp_fmt_cstr(lanes[it]));
      sp_sys_exit(1);
    }
    if (sp_cstr_equal(lanes[it], toolchain->name)) {
      return sp_str_lit("");
    }
  }
  return sp_fmt(mem, "not in lane {}", sp_fmt_str(sp_str_join_cstr_n(mem, lanes, count, sp_str_lit(", ")))).value;
}

sp_str_t test_when_blocked(test_when_t when) {
  sp_mem_t mem = sp_mem_os_new();
  const test_toolchain_t* toolchain = test_toolchain();
  spn_triple_t target = when_target(&when);

  u32 num_lanes = 0;
  sp_carr_detect_len(when.lanes, num_lanes, when.lanes[num_lanes]);
  if (num_lanes) {
    sp_str_t blocked = not_in_lanes(mem, toolchain, when.lanes, num_lanes);
    if (!sp_str_empty(blocked)) {
      return blocked;
    }
  }

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

  spn_ld_family_t family = spn_ld_family(toolchain->info->driver, toolchain->info->linker, target);
  if (when.linker && when.linker != family) {
    return sp_fmt(mem, "{} links {} with {}, test needs {}",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target)),
      sp_fmt_str(spn_ld_family_to_str(family)),
      sp_fmt_str(spn_ld_family_to_str(when.linker))).value;
  }

  spn_toolchain_selection_t selection = sp_zero;
  spn_err_t select = lane_selects(mem, &when, target, &selection);
  if (select) {
    sp_str_t request = when.sanitize ? sp_fmt(mem, " with sanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, when.sanitize))).value : sp_str_lit("");
    return sp_fmt(mem, "{} can't build {}{}: {}",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target)),
      sp_fmt_str(request),
      sp_fmt_cstr(select_reason(select))).value;
  }
  sp_str_t broken = lane_broken(mem, selection.toolchain);
  if (!sp_str_empty(broken)) {
    return sp_fmt(mem, "{} {}", sp_fmt_str(selection.toolchain->name), sp_fmt_str(broken)).value;
  }

  if (when.cxx && spn_arg_empty(toolchain->info->cxx.program)) {
    return sp_fmt(mem, "{} has no C++ compiler", sp_fmt_cstr(toolchain->name)).value;
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

sp_str_t test_lanes_toml(void) {
  sp_test_once(&once, load_lanes, SP_NULLPTR);
  return toml;
}
