#ifndef SPN_SP_IO_H
#define SPN_SP_IO_H

void sp_io_write_new_line(sp_io_writer_t* io);
void sp_io_write_line(sp_io_writer_t* io, sp_str_t line);

#ifdef SP_IO_IMPLEMENTATION

void sp_io_write_new_line(sp_io_writer_t* io) {
  sp_io_write_str(io, sp_str_lit("\n"));
}

void sp_io_write_line(sp_io_writer_t* io, sp_str_t line) {
  sp_io_write_str(io, line);
  sp_io_write_new_line(io);
}

#endif

#endif
