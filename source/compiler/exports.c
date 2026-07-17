#include "compiler/exports.h"

#include "sp/io.h"
#include "sp/macro.h"

void spn_exports_render_version_script(sp_io_writer_t* io, sp_da(sp_str_t) symbols) {
  sp_io_write_cstr(io, "{", SP_NULLPTR);
  sp_io_write_new_line(io);
  if (!sp_da_empty(symbols)) {
    sp_io_write_cstr(io, "global:", SP_NULLPTR);
    sp_io_write_new_line(io);
    sp_da_for(symbols, it) {
      sp_fmt_io(io, "  {};", sp_fmt_str(symbols[it]));
      sp_io_write_new_line(io);
    }
  }
  sp_io_write_cstr(io, "local:", SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_cstr(io, "  *;", SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_cstr(io, "};", SP_NULLPTR);
  sp_io_write_new_line(io);
}

void spn_exports_render_symbol_list(sp_io_writer_t* io, sp_da(sp_str_t) symbols) {
  sp_da_for(symbols, it) {
    sp_io_write_str(io, symbols[it], SP_NULLPTR);
    sp_io_write_new_line(io);
  }
}

void spn_exports_render_def(sp_io_writer_t* io, sp_str_t library, sp_da(sp_str_t) symbols) {
  sp_fmt_io(io, "LIBRARY {}", sp_fmt_str(library));
  sp_io_write_new_line(io);
  sp_io_write_cstr(io, "EXPORTS", SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_da_for(symbols, it) {
    sp_fmt_io(io, "  {}", sp_fmt_str(symbols[it]));
    sp_io_write_new_line(io);
  }
}
