#ifndef SPN_SHELL_SHELL_H
#define SPN_SHELL_SHELL_H

#include "sp.h"
#include "shell/types.h"

void spn_shell_flush();
void spn_print(const c8* fmt, ...);
void spn_print_err(const c8* fmt, ...);

#endif
