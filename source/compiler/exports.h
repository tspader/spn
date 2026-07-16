#ifndef SPN_COMPILER_EXPORTS_H
#define SPN_COMPILER_EXPORTS_H

#include "sp.h"
#include "spn.h"

#include "compiler/toc.h"

sp_da(sp_str_t) spn_exports_collect(sp_mem_t mem, sp_da(spn_toc_t) tocs);
sp_str_t spn_exports_render_version_script(sp_mem_t mem, sp_da(sp_str_t) symbols);
sp_str_t spn_exports_render_symbol_list(sp_mem_t mem, sp_da(sp_str_t) symbols);
sp_str_t spn_exports_render_def(sp_mem_t mem, sp_str_t library, sp_da(sp_str_t) symbols);

#endif
