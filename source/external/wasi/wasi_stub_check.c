#define SP_IMPLEMENTATION
#include "sp.h"

#define SPN_WASI_IMPLEMENTATION
#include "external/wasi/wasi.h"

int main(void) {
  sp_mem_t mem = sp_mem_os_new();
  spn_wasi_t* w = spn_wasi_new(mem);
  spn_wasi_add_preopen(w, sp_str_lit("/"), sp_str_lit("/tmp"));

  s32 fd = 0;
  spn_wasi_errno_t err = spn_wasi_path_open(w, 3, sp_str_lit("foo.txt"), 0, 0, 0, 0, &fd);
  if (err != SPN_WASI_ESUCCESS || fd != 3) {
    return 1;
  }
  if (spn_wasi_fd_close(w, fd) != SPN_WASI_ESUCCESS) {
    return 1;
  }

  spn_wasi_free(w);
  return 0;
}
