#include "caps.h"
#include "sp/sp_test.h"
#include "fixture.h"

#include "enum/enum.h"
#include "paths/paths.h"
#include "toolchain/catalog.h"
#include "toolchain/linker.h"
#include "toolchain/search.h"
#include "triple/triple.h"
#include "yyjson.h"

#define SPN_TEST_BUILTINS "source/core/toolchain/toolchains.json"
#define SPN_TEST_LANES "test/tools/toolchains.json"

static sp_test_once_t once;
static test_toolchain_t cached;
static spn_toolchain_catalog_t catalog;
static sp_str_t lanes_toml;

static sp_str_t read_repo_file(sp_mem_t mem, const c8* rel) {
  sp_str_t content = sp_zero;
  sp_assert(!sp_io_read_file(mem, test_repo_path(mem, sp_cstr_as_str(rel)), &content));
  return content;
}

static bool targets(const spn_toolchain_info_t* info, spn_triple_t triple) {
  sp_da_for(info->targets, it) {
    if (spn_triple_equal(info->targets[it].triple, triple)) {
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
  sp_da_for(info->targets, it) {
    if (info->targets[it].triple.arch == host.arch && info->targets[it].triple.os == host.os) {
      return info->targets[it].triple;
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

static bool triple_agrees(spn_triple_t a, spn_triple_t b) {
  bool arch = !a.arch || !b.arch || a.arch == b.arch;
  bool os = !a.os || !b.os || a.os == b.os;
  bool abi = !a.abi || !b.abi || a.abi == b.abi;
  return arch && os && abi;
}

static bool toolchain_targets(const spn_toolchain_info_t* info, spn_triple_t target) {
  sp_da_for(info->targets, it) {
    if (triple_agrees(info->targets[it].triple, target)) {
      return true;
    }
  }
  return false;
}

const c8* test_host_triple(void) {
  sp_mem_t mem = sp_mem_os_new();
  return sp_str_to_cstr(mem, spn_triple_to_str(mem, test_host()));
}

const c8* test_target_alternate(void) {
  const spn_toolchain_info_t* info = test_toolchain()->info;
  spn_triple_t host = test_host();

  sp_da_for(info->targets, it) {
    spn_triple_t target = info->targets[it].triple;
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

// A default build asks for musl before the host libc, so a lane that lists
// musl builds it, and builds it statically
static spn_triple_t default_target(const spn_toolchain_info_t* info) {
  spn_triple_t host = test_host();
  spn_triple_t musl = { host.arch, SPN_OS_LINUX, SPN_ABI_MUSL };
  return host.os == SPN_OS_LINUX && targets(info, musl) ? musl : host;
}

// A system gcc or clang carries sanitizer runtimes for the host libc only; a
// lane that swaps the libc through a sysroot or a wrapper can't link them
static bool toolchain_sanitizes(const test_toolchain_t* toolchain, spn_triple_t target) {
  switch (toolchain->info->driver) {
    case SPN_CC_DRIVER_GCC:
    case SPN_CC_DRIVER_CLANG: return target.os != SPN_OS_LINUX || target.abi == spn_triple_host().abi;
    case SPN_CC_DRIVER_ZIG:
    case SPN_CC_DRIVER_MSVC: return true;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  sp_unreachable_return(true);
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

typedef struct {
  const c8* lane;
  spn_ld_flavor_t flavor;
  const c8* program;
} lane_program_t;

static const lane_program_t lane_programs [] = {
  { "llvm",            SPN_LD_FLAVOR_ELF,   "ld.lld" },
  { "llvm",            SPN_LD_FLAVOR_MACHO, "ld64.lld" },
  { "gcc-lld",         SPN_LD_FLAVOR_ELF,   "ld.lld" },
  { "clang-msvc",      SPN_LD_FLAVOR_MSVC,  "lld-link" },
  { "clang-mingw",     SPN_LD_FLAVOR_MINGW, "x86_64-w64-mingw32-ld" },
};

static bool links_flavor(const spn_toolchain_info_t* info, spn_ld_flavor_t flavor) {
  sp_da_for(info->targets, it) {
    if (spn_ld_flavor(info->targets[it].triple) == flavor) {
      return true;
    }
  }
  return false;
}

static sp_str_t missing_lane_program(sp_mem_t mem, const spn_toolchain_info_t* info) {
  sp_carr_for(lane_programs, it) {
    const lane_program_t* row = &lane_programs[it];
    if (!sp_str_equal_cstr(info->name, row->lane) || !links_flavor(info, row->flavor)) {
      continue;
    }
    if (!installed(mem, sp_cstr_as_str(row->program))) {
      return sp_cstr_as_str(row->program);
    }
  }
  return sp_str_lit("");
}

static sp_str_t missing_toolchain_program(sp_mem_t mem, const spn_toolchain_info_t* info) {
  sp_str_t programs [] = {
    info->compiler.program.prefix,
    info->archiver.program.prefix,
  };
  sp_carr_for(programs, it) {
    if (!sp_str_empty(programs[it]) && !installed(mem, programs[it])) {
      return programs[it];
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

static void write_str(sp_io_writer_t* io, const c8* key, yyjson_val* value) {
  sp_fmt_io(io, "{} = \"{}\"\n", sp_fmt_cstr(key), sp_fmt_cstr(yyjson_get_str(value)));
}

static void write_launcher(sp_io_writer_t* io, const c8* key, yyjson_val* launcher) {
  sp_fmt_io(io, "{} = \"{}", sp_fmt_cstr(key), sp_fmt_cstr(yyjson_get_str(yyjson_obj_get(launcher, "program"))));
  size_t idx, max;
  yyjson_val* arg;
  yyjson_arr_foreach(yyjson_obj_get(launcher, "args"), idx, max, arg) {
    sp_fmt_io(io, " {}", sp_fmt_cstr(yyjson_get_str(arg)));
  }
  sp_io_write_cstr(io, "\"\n", SP_NULLPTR);
}

static void write_table(sp_io_writer_t* io, yyjson_val* obj) {
  sp_io_write_cstr(io, "{", SP_NULLPTR);
  size_t idx, max;
  yyjson_val* key;
  yyjson_val* value;
  yyjson_obj_foreach(obj, idx, max, key, value) {
    sp_fmt_io(io, "{} {} = \"{}\"", sp_fmt_cstr(idx ? "," : ""), sp_fmt_cstr(yyjson_get_str(key)), sp_fmt_cstr(yyjson_get_str(value)));
  }
  sp_io_write_cstr(io, " }", SP_NULLPTR);
}

static void write_tables(sp_io_writer_t* io, const c8* name, yyjson_val* obj) {
  sp_fmt_io(io, "{} = ", sp_fmt_cstr(name));
  sp_io_write_cstr(io, "{", SP_NULLPTR);
  size_t idx, max;
  yyjson_val* key;
  yyjson_val* value;
  yyjson_obj_foreach(obj, idx, max, key, value) {
    sp_fmt_io(io, "{} {} = ", sp_fmt_cstr(idx ? "," : ""), sp_fmt_cstr(yyjson_get_str(key)));
    write_table(io, value);
  }
  sp_io_write_cstr(io, " }\n", SP_NULLPTR);
}

static void write_str_array(sp_io_writer_t* io, const c8* name, yyjson_val* arr) {
  sp_fmt_io(io, "{} = [", sp_fmt_cstr(name));
  size_t idx, max;
  yyjson_val* value;
  yyjson_arr_foreach(arr, idx, max, value) {
    sp_fmt_io(io, "{} \"{}\"", sp_fmt_cstr(idx ? "," : ""), sp_fmt_cstr(yyjson_get_str(value)));
  }
  sp_io_write_cstr(io, " ]\n", SP_NULLPTR);
}

static void write_table_array(sp_io_writer_t* io, const c8* name, yyjson_val* arr) {
  sp_fmt_io(io, "{} = [", sp_fmt_cstr(name));
  size_t idx, max;
  yyjson_val* value;
  yyjson_arr_foreach(arr, idx, max, value) {
    sp_io_write_cstr(io, idx ? ", " : " ", SP_NULLPTR);
    write_table(io, value);
  }
  sp_io_write_cstr(io, " ]\n", SP_NULLPTR);
}

static void write_lane(sp_io_writer_t* io, yyjson_val* toolchain) {
  sp_io_write_cstr(io, "\n[[toolchain]]\n", SP_NULLPTR);
  write_str(io, "name", yyjson_obj_get(toolchain, "name"));
  write_str(io, "driver", yyjson_obj_get(toolchain, "driver"));
  write_launcher(io, "compiler", yyjson_obj_get(toolchain, "compiler"));
  write_launcher(io, "archiver", yyjson_obj_get(toolchain, "archiver"));
  if (yyjson_obj_get(toolchain, "cxx")) {
    write_launcher(io, "cxx", yyjson_obj_get(toolchain, "cxx"));
  }
  if (yyjson_obj_get(toolchain, "linker")) {
    sp_io_write_cstr(io, "linker = ", SP_NULLPTR);
    write_table(io, yyjson_obj_get(toolchain, "linker"));
    sp_io_write_cstr(io, "\n", SP_NULLPTR);
  }
  if (yyjson_obj_get(toolchain, "link_args")) {
    write_str_array(io, "link_args", yyjson_obj_get(toolchain, "link_args"));
  }
  if (yyjson_obj_get(toolchain, "host")) {
    write_tables(io, "host", yyjson_obj_get(toolchain, "host"));
  }
  if (yyjson_obj_get(toolchain, "target")) {
    write_table_array(io, "target", yyjson_obj_get(toolchain, "target"));
  }
  if (yyjson_obj_get(toolchain, "mirrors")) {
    write_str(io, "mirrors", yyjson_obj_get(toolchain, "mirrors"));
  }
}

static sp_str_t render_lanes(sp_mem_t mem, sp_str_t json) {
  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  sp_assert(doc);

  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &writer);
  size_t idx, max;
  yyjson_val* toolchain;
  yyjson_arr_foreach(yyjson_obj_get(yyjson_doc_get_root(doc), "toolchain"), idx, max, toolchain) {
    write_lane(&writer.base, toolchain);
  }
  yyjson_doc_free(doc);
  return sp_io_dyn_mem_writer_as_str(&writer);
}

static sp_err_t load_lanes(void* user) {
  sp_mem_t mem = sp_mem_os_new();
  sp_str_t name = sp_os_env_get(sp_str_lit("SPN_TEST_TOOLCHAIN"));
  if (sp_str_empty(name)) {
    name = sp_str_lit("zig");
  }

  sp_str_t lanes = read_repo_file(mem, SPN_TEST_LANES);
  spn_toolchain_catalog_init(&catalog, spn_triple_host(), mem);
  sp_assert(spn_toolchain_catalog_load(&catalog, read_repo_file(mem, SPN_TEST_BUILTINS)) == SPN_OK);
  sp_assert(spn_toolchain_catalog_load(&catalog, lanes) == SPN_OK);
  lanes_toml = render_lanes(mem, lanes);

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

  spn_ld_family_t family = toolchain->info->linkers.families[spn_ld_flavor(target)];
  if (when.linker && when.linker != family) {
    return sp_fmt(mem, "{} links {} with {}, test needs {}",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target)),
      sp_fmt_str(spn_ld_family_to_str(family)),
      sp_fmt_str(spn_ld_family_to_str(when.linker))).value;
  }

  if (!toolchain_targets(toolchain->info, target)) {
    return sp_fmt(mem, "{} can't target {}",
      sp_fmt_cstr(toolchain->name),
      sp_fmt_str(spn_triple_to_str(mem, target))).value;
  }

  if (when.sanitize) {
    spn_triple_t picked = when.target ? target : default_target(toolchain->info);
    spn_cc_toolchain_t cc = {
      .name = toolchain->info->name,
      .driver = toolchain->info->driver,
    };
    if (when.sanitize & ~get_supported_sanitizers(&cc, picked)) {
      return sp_fmt(mem, "{} targeting {} can't build sanitize={}",
        sp_fmt_cstr(toolchain->name),
        sp_fmt_str(spn_triple_to_str(mem, picked)),
        sp_fmt_str(spn_sanitizer_set_to_str(mem, when.sanitize))).value;
    }
    if (picked.abi == SPN_ABI_MUSL && (when.sanitize & ~SPN_SANITIZER_UNDEFINED)) {
      return sp_fmt(mem, "{} links {} statically, which refuses sanitize={}",
        sp_fmt_cstr(toolchain->name),
        sp_fmt_str(spn_triple_to_str(mem, picked)),
        sp_fmt_str(spn_sanitizer_set_to_str(mem, when.sanitize))).value;
    }
    if (!toolchain_sanitizes(toolchain, picked)) {
      return sp_fmt(mem, "{} targeting {} has no sanitizer runtime for that libc",
        sp_fmt_cstr(toolchain->name),
        sp_fmt_str(spn_triple_to_str(mem, picked))).value;
    }
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
  return lanes_toml;
}
