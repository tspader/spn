#include "log/lazy/lazy.h"

static sp_err_t spn_lazy_log_write(sp_io_writer_t* w, const void* ptr, u64 size, u64* bytes_written) {
  spn_lazy_log_t* log = (spn_lazy_log_t*)w;
  if (!log->opened) {
    log->file = sp_io_writer_from_file(log->path, log->mode);
    log->opened = true;
  }
  if (!log->file) return SP_ERR_IO;
  return sp_io_write(log->file, ptr, size, bytes_written);
}

void spn_lazy_log_init(spn_lazy_log_t* log, sp_str_t path, sp_io_write_mode_t mode) {
  *log = (spn_lazy_log_t) {
    .writer = { .write = spn_lazy_log_write },
    .path = path,
    .mode = mode,
  };
}
