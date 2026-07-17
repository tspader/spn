#ifndef SPN_COMPILER_EXPORTS_H
#define SPN_COMPILER_EXPORTS_H

#include "sp.h"
#include "spn.h"

void spn_exports_render_version_script(sp_io_writer_t* io, sp_da(sp_str_t) symbols);
void spn_exports_render_symbol_list(sp_io_writer_t* io, sp_da(sp_str_t) symbols);
void spn_exports_render_def(sp_io_writer_t* io, sp_str_t library, sp_da(sp_str_t) symbols);

#endif
