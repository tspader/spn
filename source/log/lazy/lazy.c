#include "log/lazy/lazy.h"

static u64 spn_lazy_log_write(sp_io_writer_t* w, const void* ptr, u64 size) {
  spn_lazy_log_t* log = (spn_lazy_log_t*)w;
  if (!log->opened) {
    sp_io_writer_t file = sp_io_writer_from_file(log->path, log->mode);
    if (!file.vtable.write) return 0;
    log->writer.vtable = file.vtable;
    log->writer.file = file.file;
    log->opened = true;
  }
  return log->writer.vtable.write(w, ptr, size);
}

void spn_lazy_log_init(spn_lazy_log_t* log, sp_str_t path, sp_io_write_mode_t mode) {
  *log = (spn_lazy_log_t) {
    .writer = { .vtable = { .write = spn_lazy_log_write } },
    .path = path,
    .mode = mode,
  };
}
