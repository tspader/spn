#include "io.h"
#include "ctx/types.h"

void sp_io_write_new_line(sp_io_writer_t* io) {
  sp_io_write_str(io, sp_str_lit("\n"));
}

void sp_io_write_line(sp_io_writer_t* io, sp_str_t line) {
  sp_io_write_str(io, line);
  sp_io_write_new_line(io);
}

void sp_io_write_u8(sp_io_writer_t* io, u8 value) {
  sp_io_write(io, &value, sizeof(u8));
}

void sp_io_write_u16(sp_io_writer_t* io, u16 value) {
  sp_io_write(io, &value, sizeof(u16));
}

void sp_io_write_u32(sp_io_writer_t* io, u32 value) {
  sp_io_write(io, &value, sizeof(u32));
}

void sp_io_write_u64(sp_io_writer_t* io, u64 value) {
  sp_io_write(io, &value, sizeof(u64));
}

void sp_io_write_s16(sp_io_writer_t* io, s16 value) {
  sp_io_write(io, &value, sizeof(s16));
}

sp_mem_buffer_t sp_io_read_file_as_buffer(sp_str_t path) {
  sp_mem_buffer_t buffer = sp_zero_struct(sp_mem_buffer_t);

  sp_io_reader_t reader = sp_io_reader_from_file(path);
  if (reader.file.fd <= 0) {
    return buffer;
  }

  u64 size = sp_io_reader_size(&reader);
  if (size == 0) {
    sp_io_reader_close(&reader);
    return buffer;
  }

  buffer.data = sp_alloc_n(u8, size);
  buffer.capacity = size;
  buffer.len = sp_io_read(&reader, buffer.data, buffer.capacity);
  sp_io_reader_close(&reader);

  return buffer;
}

sp_mem_buffer_t sp_io_read_all(sp_io_reader_t* io) {
  u64 size = sp_io_reader_size(io);
  sp_io_reader_seek(io, 0, SP_IO_SEEK_SET);

  sp_mem_buffer_t buffer = {
    .data = sp_alloc_n(u8, size),
    .capacity = size,
  };
  buffer.len = sp_io_read(io, buffer.data, buffer.capacity);
  return buffer;
}

