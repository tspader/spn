#ifndef SPN_SHELL_TYPES_H
#define SPN_SHELL_TYPES_H

#include "sp.h"

#include "cli/types.h"
#include "forward/types.h"
#include "tui/types.h"

struct spn_shell_t {
  spn_cli_t cli;
  spn_tui_t tui;
  spn_logger_t logger;
};

extern spn_shell_t shell;

#endif
