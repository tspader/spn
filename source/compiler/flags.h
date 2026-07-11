#ifndef spn_compiler_flags_h
#define spn_compiler_flags_h

#include "compiler/types.h"
#include "error/types.h"
#include "profile/types.h"
#include "toolchain/types.h"

spn_err_union_t spn_cc_flags_resolve(
  sp_mem_t mem,
  const spn_profile_info_t* profile,
  const spn_toolchain_t* toolchain,
  spn_cc_flags_t* flags
);

#endif
