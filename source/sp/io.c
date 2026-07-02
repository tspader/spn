#include "io.h"

void sp_io_write_new_line(sp_io_writer_t* io) {
  sp_io_write_str(io, sp_str_lit("\n"), SP_NULLPTR);
}

void sp_io_write_line(sp_io_writer_t* io, sp_str_t line) {
  sp_io_write_str(io, line, SP_NULLPTR);
  sp_io_write_new_line(io);
}

void sp_io_write_u8(sp_io_writer_t* io, u8 value) {
  sp_io_write(io, &value, sizeof(u8), SP_NULLPTR);
}

void sp_io_write_u16(sp_io_writer_t* io, u16 value) {
  sp_io_write(io, &value, sizeof(u16), SP_NULLPTR);
}

void sp_io_write_u32(sp_io_writer_t* io, u32 value) {
  sp_io_write(io, &value, sizeof(u32), SP_NULLPTR);
}

void sp_io_write_u64(sp_io_writer_t* io, u64 value) {
  sp_io_write(io, &value, sizeof(u64), SP_NULLPTR);
}

void sp_io_write_s16(sp_io_writer_t* io, s16 value) {
  sp_io_write(io, &value, sizeof(s16), SP_NULLPTR);
}

