#include "lanes.h"

#include "enum/enum.h"
#include "toolchain/toolchain.h"

const spn_cg_toolchain_t* lanes_find(const spn_cg_toolchains_t* lanes, sp_str_t name) {
  spn_cg_toolchain_t** entry = sp_str_om_getp(lanes->toolchain, name);
  return entry ? *entry : SP_NULLPTR;
}

const spn_cg_artifact_t* lane_artifact(const spn_cg_toolchain_t* lane, sp_str_t host) {
  sp_da_for(lane->host, it) {
    if (sp_str_equal(lane->host[it].key, host)) {
      return &lane->host[it].value;
    }
  }
  return SP_NULLPTR;
}

static void write_str(sp_io_writer_t* io, const c8* key, sp_str_t value) {
  sp_fmt_io(io, "{} = \"{}\"\n", sp_fmt_cstr(key), sp_fmt_str(value));
}

static void write_launcher(sp_io_writer_t* io, const c8* key, const spn_cg_launcher_t* launcher) {
  sp_fmt_io(io, "{} = \"{}", sp_fmt_cstr(key), sp_fmt_str(launcher->program));
  sp_da_for(launcher->args, it) {
    sp_fmt_io(io, " {}", sp_fmt_str(launcher->args[it]));
  }
  sp_io_write_cstr(io, "\"\n", SP_NULLPTR);
}

static void write_str_array(sp_io_writer_t* io, const c8* key, sp_da(sp_str_t) values) {
  sp_fmt_io(io, "{} = [", sp_fmt_cstr(key));
  sp_da_for(values, it) {
    sp_fmt_io(io, "{} \"{}\"", sp_fmt_cstr(it ? "," : ""), sp_fmt_str(values[it]));
  }
  sp_io_write_cstr(io, " ]\n", SP_NULLPTR);
}

static void write_field(sp_io_writer_t* io, bool* first, const c8* key, sp_str_t value) {
  sp_fmt_io(io, "{} {} = \"{}\"", sp_fmt_cstr(*first ? "" : ","), sp_fmt_cstr(key), sp_fmt_str(value));
  *first = false;
}

static void write_artifact(sp_io_writer_t* io, const spn_cg_artifact_t* artifact) {
  bool first = true;
  sp_io_write_cstr(io, "{", SP_NULLPTR);
  if (!sp_str_empty(artifact->url)) {
    write_field(io, &first, "url", artifact->url);
  }
  if (!sp_str_empty(artifact->sha256)) {
    write_field(io, &first, "sha256", artifact->sha256);
  }
  sp_io_write_cstr(io, " }", SP_NULLPTR);
}

static void write_sanitizer_field(sp_io_writer_t* io, bool* first, spn_sanitizer_set_t set) {
  sp_fmt_io(io, "{} sanitizers = [", sp_fmt_cstr(*first ? "" : ","));
  *first = false;
  bool inner = true;
  while (set) {
    spn_sanitizer_set_t bit = set & (~set + 1);
    sp_fmt_io(io, "{} \"{}\"", sp_fmt_cstr(inner ? "" : ","), sp_fmt_str(spn_sanitizer_to_str((spn_sanitizer_t)bit)));
    inner = false;
    set &= set - 1;
  }
  sp_io_write_cstr(io, " ]", SP_NULLPTR);
}

static void write_target(sp_io_writer_t* io, const spn_cg_toolchain_target_t* target) {
  bool first = true;
  spn_sanitizer_set_t sanitizers = 0;
  sp_da_for(target->sanitizers, it) {
    sanitizers |= target->sanitizers[it];
  }
  sp_io_write_cstr(io, "{", SP_NULLPTR);
  if (!sp_opt_is_null(target->arch)) {
    write_field(io, &first, "arch", spn_arch_to_str(sp_opt_get(target->arch)));
  }
  if (!sp_opt_is_null(target->os)) {
    write_field(io, &first, "os", spn_os_to_str(sp_opt_get(target->os)));
  }
  if (!sp_opt_is_null(target->abi)) {
    write_field(io, &first, "abi", spn_abi_to_str(sp_opt_get(target->abi)));
  }
  if (!sp_str_empty(target->sdk)) {
    write_field(io, &first, "sdk", target->sdk);
  }
  if (sanitizers) {
    write_sanitizer_field(io, &first, sanitizers);
  }
  sp_io_write_cstr(io, " }", SP_NULLPTR);
}

static void write_row(sp_io_writer_t* io, const spn_toolchain_row_t* row) {
  bool first = true;
  sp_io_write_cstr(io, "{", SP_NULLPTR);
  write_field(io, &first, "arch", spn_arch_to_str(row->triple.arch));
  write_field(io, &first, "os", spn_os_to_str(row->triple.os));
  write_field(io, &first, "abi", spn_abi_to_str(row->triple.abi));
  if (!spn_sdk_host_reachable(row->triple)) {
    write_field(io, &first, "sdk", sp_str_lit("toolchain"));
  }
  if (row->sanitizers) {
    write_sanitizer_field(io, &first, row->sanitizers);
  }
  sp_io_write_cstr(io, " }", SP_NULLPTR);
}

static void write_lane(sp_io_writer_t* io, const spn_cg_toolchain_t* lane, const spn_toolchain_info_t* info) {
  sp_io_write_cstr(io, "\n[[toolchain]]\n", SP_NULLPTR);
  write_str(io, "name", lane->name);
  write_str(io, "driver", spn_cc_driver_to_str(lane->driver));
  write_launcher(io, "compiler", &lane->compiler);
  write_launcher(io, "archiver", &lane->archiver);
  if (!sp_str_empty(lane->cxx.program)) {
    write_launcher(io, "cxx", &lane->cxx);
  }
  if (!sp_opt_is_null(lane->linker)) {
    write_str(io, "linker", spn_ld_family_to_str(sp_opt_get(lane->linker)));
  }
  if (!sp_da_empty(lane->link_args)) {
    write_str_array(io, "link_args", lane->link_args);
  }
  if (!sp_da_empty(lane->host)) {
    sp_io_write_cstr(io, "host = {", SP_NULLPTR);
    sp_da_for(lane->host, it) {
      sp_fmt_io(io, "{} {} = ", sp_fmt_cstr(it ? "," : ""), sp_fmt_str(lane->host[it].key));
      write_artifact(io, &lane->host[it].value);
    }
    sp_io_write_cstr(io, " }\n", SP_NULLPTR);
  }
  if (!sp_da_empty(lane->target)) {
    sp_io_write_cstr(io, "target = [", SP_NULLPTR);
    sp_da_for(lane->target, it) {
      sp_io_write_cstr(io, it ? ", " : " ", SP_NULLPTR);
      write_target(io, &lane->target[it]);
    }
    sp_io_write_cstr(io, " ]\n", SP_NULLPTR);
  } else if (info && !sp_da_empty(info->rows)) {
    sp_io_write_cstr(io, "target = [", SP_NULLPTR);
    sp_da_for(info->rows, it) {
      sp_io_write_cstr(io, it ? ", " : " ", SP_NULLPTR);
      write_row(io, &info->rows[it]);
    }
    sp_io_write_cstr(io, " ]\n", SP_NULLPTR);
  }
  if (!sp_str_empty(lane->mirrors)) {
    write_str(io, "mirrors", lane->mirrors);
  }
}

sp_str_t lanes_toml(sp_mem_t mem, spn_toolchain_catalog_t* catalog, const spn_cg_toolchains_t* lanes) {
  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &writer);
  sp_om_for(lanes->toolchain, it) {
    const spn_cg_toolchain_t* lane = sp_om_at(lanes->toolchain, it);
    write_lane(&writer.base, lane, spn_toolchain_catalog_get(catalog, lane->name));
  }
  return sp_io_dyn_mem_writer_as_str(&writer);
}
