#include "compiler/toc.h"

spn_err_union_t spn_toc_read(sp_mem_t mem, sp_str_t archive, spn_toc_t* toc) {
  (void)archive;
  sp_da_init(mem, toc->symbols);
  return spn_result(SPN_ERROR);
}
