#ifndef SPN_SHELL_SHELL_H
#define SPN_SHELL_SHELL_H

#include "sp.h"
#include "shell/types.h"

void spn_shell_init(spn_shell_t* shell);
void spn_shell_open(spn_shell_t* shell);
void spn_shell_open_log(spn_shell_t* shell, sp_str_t dir);
void spn_shell_flush();
void spn_print(const c8* fmt, ...);
void spn_print_err(const c8* fmt, ...);

#endif
