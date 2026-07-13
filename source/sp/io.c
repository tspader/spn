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


sp_err_t sp_io_read_file_slice(sp_mem_t mem, sp_str_t path, sp_mem_slice_t* content) {
  sp_assert(content);
  sp_err_t err = SP_OK;
  u8* buffer = SP_NULLPTR;
  u64 size = 0;
  u64 bytes_read = 0;

  sp_io_file_reader_t reader = sp_zero;
  sp_try(sp_io_file_reader_from_path(&reader, path));

  sp_try_goto(sp_io_file_reader_size(&reader, &size), err, cleanup);
  if (!size) goto cleanup;

  buffer = sp_alloc_n(mem, u8, size);
  sp_try_goto(sp_io_read(&reader.base, buffer, size, &bytes_read), err, cleanup);
  if (bytes_read < size) {
    buffer = (u8*)sp_realloc(mem, buffer, size, bytes_read);
  }
  content->data = buffer;
  content->len = bytes_read;
  buffer = SP_NULLPTR;

cleanup:
  if (buffer) sp_mem_allocator_free(mem, buffer, size);
  sp_io_file_reader_close(&reader);
  return err;
}
