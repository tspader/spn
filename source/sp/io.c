#include "io.h"
#include "ctx/types.h"

sp_io_writer_t* sp_io_writer_from_file(sp_str_t path, sp_io_write_mode_t mode) {
  sp_io_file_writer_t* w = sp_alloc_type(spn_allocator, sp_io_file_writer_t);

  if (mode == SP_IO_WRITE_MODE_APPEND) {
    s32 flags = SP_O_WRONLY | SP_O_CREAT | SP_O_APPEND | SP_O_BINARY;
    sp_sys_fd_t fd = sp_sys_open_s(sp_sys_get_root(0), path, flags, 0644);
    if (fd == SP_SYS_INVALID_FD) {
      *w = sp_zero_s(sp_io_file_writer_t);
      return &w->base;
    }
    sp_io_file_writer_from_fd(w, fd, SP_IO_CLOSE_MODE_AUTO);
  } else {
    sp_io_file_writer_from_path(w, path);
  }

  return &w->base;
}

sp_io_writer_t* sp_io_writer_from_fd(s32 fd, sp_io_close_mode_t close_mode) {
  sp_io_stream_writer_t* w = sp_alloc_type(spn_allocator, sp_io_stream_writer_t);
  sp_io_stream_writer_from_fd(w, (sp_sys_fd_t)fd, close_mode);
  return &w->base;
}

sp_io_writer_t* sp_io_writer_from_dyn_mem(void) {
  sp_io_dyn_mem_writer_t* w = sp_alloc_type(spn_allocator, sp_io_dyn_mem_writer_t);
  sp_io_dyn_mem_writer_init(spn_allocator, w);
  return &w->base;
}

void sp_io_writer_close(sp_io_writer_t* w) {
  if (!w) return;
  sp_io_file_writer_close((sp_io_file_writer_t*)w);
}

u64 sp_io_writer_size(sp_io_writer_t* w) {
  u64 size = 0;
  sp_io_file_writer_size((sp_io_file_writer_t*)w, &size);
  return size;
}

sp_str_t sp_io_writer_to_str(sp_io_writer_t* w) {
  return sp_io_dyn_mem_writer_as_str((sp_io_dyn_mem_writer_t*)w);
}

sp_io_reader_t* sp_io_reader_from_file(sp_str_t path) {
  sp_io_file_reader_t* r = sp_alloc_type(spn_allocator, sp_io_file_reader_t);
  sp_io_file_reader_from_path(r, path);
  return &r->base;
}

u64 sp_io_reader_size(sp_io_reader_t* r) {
  u64 size = 0;
  sp_io_file_reader_size((sp_io_file_reader_t*)r, &size);
  return size;
}

void sp_io_reader_close(sp_io_reader_t* r) {
  if (!r) return;
  sp_io_file_reader_close((sp_io_file_reader_t*)r);
}

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

sp_mem_buffer_t sp_io_read_file_as_buffer(sp_str_t path) {
  sp_mem_buffer_t buffer = sp_zero_s(sp_mem_buffer_t);

  sp_str_t content = sp_zero;
  if (sp_io_read_file(spn_allocator, path, &content) != SP_OK) {
    return buffer;
  }

  buffer.data = (u8*)content.data;
  buffer.len = content.len;
  buffer.capacity = content.len;
  return buffer;
}

sp_mem_buffer_t sp_io_read_all(sp_io_reader_t* io) {
  u64 size = sp_io_reader_size(io);

  sp_mem_buffer_t buffer = {
    .data = sp_alloc_n(spn_allocator, u8, size),
    .capacity = size,
  };
  sp_io_read(io, buffer.data, buffer.capacity, &buffer.len);
  return buffer;
}
