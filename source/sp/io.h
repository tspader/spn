#ifndef SPN_SP_IO_H
#define SPN_SP_IO_H

#include "sp/compat.h"

void sp_io_write_new_line(sp_io_writer_t* io);
void sp_io_write_line(sp_io_writer_t* io, sp_str_t line);
void sp_io_write_u8(sp_io_writer_t* io, u8 value);
void sp_io_write_u16(sp_io_writer_t* io, u16 value);
void sp_io_write_u32(sp_io_writer_t* io, u32 value);
void sp_io_write_u64(sp_io_writer_t* io, u64 value);
void sp_io_write_s16(sp_io_writer_t* io, s16 value);
sp_mem_buffer_t sp_io_read_file_as_buffer(sp_str_t path);
sp_mem_buffer_t sp_io_read_all(sp_io_reader_t* io);

#endif
