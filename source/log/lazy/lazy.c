#include "log/lazy/lazy.h"

static sp_err_t spn_lazy_log_write(sp_io_writer_t* w, const void* ptr, u64 size, u64* bytes_written) {
  spn_lazy_log_t* log = (spn_lazy_log_t*)w;
  if (!log->opened) {
    log->opened = true;
    if (sp_io_file_writer_from_path(&log->file, log->path) != SP_OK) {
      log->failed = true;
    }
  }
  if (log->failed) return SP_ERR_IO;
  return sp_io_write(&log->file.base, ptr, size, bytes_written);
}

void spn_lazy_log_init(spn_lazy_log_t* log, sp_str_t path) {
  *log = (spn_lazy_log_t) {
    .writer = { .write = spn_lazy_log_write },
    .path = path,
  };
}

void spn_lazy_log_close(spn_lazy_log_t* log) {
  if (log->opened && !log->failed) {
    sp_io_file_writer_close(&log->file);
    log->opened = false;
  }
}
