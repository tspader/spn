#include "compiler/rsp.h"

#include "io/io.h"
#include "paths/paths.h"

static bool needs_quotes(spn_rsp_style_t style, sp_str_t arg) {
  if (sp_str_empty(arg) || arg.data[0] == '#') {
    return true;
  }
  sp_for(it, arg.len) {
    c8 c = arg.data[it];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '"' || c == '\'') {
      return true;
    }
    if (c == '\\' && style == SPN_RSP_STYLE_GNU) {
      return true;
    }
  }
  return false;
}

static void write_quoted_windows(sp_io_writer_t* io, sp_str_t arg) {
  sp_io_write_c8(io, '"');
  u32 backslashes = 0;
  sp_for(it, arg.len) {
    c8 c = arg.data[it];
    if (c == '\\') {
      backslashes++;
      continue;
    }
    if (c == '"') {
      backslashes = backslashes * 2 + 1;
    }
    sp_for(jt, backslashes) {
      sp_io_write_c8(io, '\\');
    }
    backslashes = 0;
    sp_io_write_c8(io, c);
  }
  sp_for(jt, backslashes * 2) {
    sp_io_write_c8(io, '\\');
  }
  sp_io_write_c8(io, '"');
}

static void write_quoted_gnu(sp_io_writer_t* io, sp_str_t arg) {
  sp_io_write_c8(io, '"');
  sp_for(it, arg.len) {
    c8 c = arg.data[it];
    if (c == '\\' || c == '"') {
      sp_io_write_c8(io, '\\');
    }
    sp_io_write_c8(io, c);
  }
  sp_io_write_c8(io, '"');
}

static void write_arg(sp_io_writer_t* io, spn_rsp_style_t style, sp_str_t arg) {
  if (!needs_quotes(style, arg)) {
    sp_io_write_str(io, arg, SP_NULLPTR);
    return;
  }
  switch (style) {
    case SPN_RSP_STYLE_WINDOWS: {
      write_quoted_windows(io, arg);
      break;
    }
    case SPN_RSP_STYLE_GNU: {
      write_quoted_gnu(io, arg);
      break;
    }
  }
}

u32 spn_rsp_cmdline_len(const spn_path_roots_t* roots, const spn_invocation_t* invocation) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_io_dyn_mem_writer_t buf;
  sp_io_dyn_mem_writer_init(s.mem, &buf);

  write_arg(&buf.base, SPN_RSP_STYLE_WINDOWS, spn_arg_str(roots, s.mem, invocation->program));
  sp_da_for(invocation->args, it) {
    sp_io_write_c8(&buf.base, ' ');
    write_arg(&buf.base, SPN_RSP_STYLE_WINDOWS, spn_arg_str(roots, s.mem, invocation->args[it]));
  }

  u32 len = sp_io_dyn_mem_writer_as_str(&buf).len;
  sp_mem_end_scratch(s);
  return len;
}

spn_rsp_style_t spn_rsp_style(spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: {
      return SPN_RSP_STYLE_GNU;
    }
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG:
    case SPN_CC_DRIVER_MSVC:
    case SPN_CC_DRIVER_NONE: {
      return SPN_RSP_STYLE_WINDOWS;
    }
  }
  sp_unreachable_return(SPN_RSP_STYLE_WINDOWS);
}

spn_rsp_t spn_rsp_render(sp_mem_t mem, const spn_path_roots_t* roots, const spn_invocation_t* invocation, spn_rsp_style_t style, spn_path_t file) {
  sp_assert(invocation->launcher <= sp_da_size(invocation->args));
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  spn_rsp_t rsp = {
    .invocation = {
      .program = invocation->program,
      .launcher = invocation->launcher,
      .cwd = invocation->cwd,
    },
  };
  sp_da_init(mem, rsp.invocation.args);
  sp_for(it, invocation->launcher) {
    sp_da_push(rsp.invocation.args, invocation->args[it]);
  }
  sp_da_push(rsp.invocation.args, spn_arg_glue(sp_str_lit("@"), file));

  sp_io_dyn_mem_writer_t buf;
  sp_io_dyn_mem_writer_init(mem, &buf);
  sp_for(it, sp_da_size(invocation->args) - invocation->launcher) {
    write_arg(&buf.base, style, spn_arg_str(roots, s.mem, invocation->args[invocation->launcher + it]));
    sp_io_write_new_line(&buf.base);
  }
  rsp.content = sp_io_dyn_mem_writer_take_str(&buf);

  sp_mem_end_scratch(s);
  return rsp;
}
