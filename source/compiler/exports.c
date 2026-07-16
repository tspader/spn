#include "compiler/exports.h"

sp_da(sp_str_t) spn_exports_collect(sp_mem_t mem, sp_da(spn_toc_t) tocs) {
  (void)tocs;
  return sp_da_new(mem, sp_str_t);
}

sp_str_t spn_exports_render_version_script(sp_mem_t mem, sp_da(sp_str_t) symbols) {
  (void)mem;
  (void)symbols;
  return sp_str_lit("");
}

sp_str_t spn_exports_render_symbol_list(sp_mem_t mem, sp_da(sp_str_t) symbols) {
  (void)mem;
  (void)symbols;
  return sp_str_lit("");
}

sp_str_t spn_exports_render_def(sp_mem_t mem, sp_str_t library, sp_da(sp_str_t) symbols) {
  (void)mem;
  (void)library;
  (void)symbols;
  return sp_str_lit("");
}
