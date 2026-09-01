#ifndef spn_compiler_rsp_h
#define spn_compiler_rsp_h

#include "compiler/types.h"
#include "paths/types.h"

#define spn_rsp_cmdline_max 32766

u32       spn_rsp_cmdline_len(const spn_path_roots_t* roots, const spn_invocation_t* invocation);
spn_rsp_t spn_rsp_render(sp_mem_t mem, const spn_path_roots_t* roots, const spn_invocation_t* invocation, spn_path_t file);

#endif
