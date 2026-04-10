#ifndef SPN_TCC_BACKTRACE_H
#define SPN_TCC_BACKTRACE_H

#include "forward/types.h"

s32 on_tcc_backtrace(void* ud, void* pc, const c8* file, s32 line, const c8* fn, const c8* message);

#endif
