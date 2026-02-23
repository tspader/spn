#ifndef SPN_PTY_H
#define SPN_PTY_H

#include "sp.h"

#ifdef SP_LINUX
s32 spn_pty_wrap(s32 num_args, const c8** args);
#endif

#endif
