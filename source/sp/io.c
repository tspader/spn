#include "io.h"

void sp_io_write_new_line(sp_io_writer_t* io) {
  sp_io_write_str(io, sp_str_lit("\n"));
}

void sp_io_write_line(sp_io_writer_t* io, sp_str_t line) {
  sp_io_write_str(io, line);
  sp_io_write_new_line(io);
}
