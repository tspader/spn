#include "sp.h"
#ifdef _WIN32
  #include <windows.h>
#endif
#include "app/app.h"
#include "ctx/ctx.h"
#include "sp/io.h"

#ifdef SP_POSIX
void spn_signal_handler(s32 kind) {
  switch (kind) {
    case SIGINT: {
      sp_atomic_s32_set(&spn.sp->shutdown, 1);
      sp_io_write_new_line(&spn.logger.out);
      sp_io_write_new_line(&spn.logger.err);
      break;
    }
    default: {
      break;
    }
  }
}

void spn_install_signal_handlers() {
  struct sigaction sa;
  sa.sa_handler = spn_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
}
#else
BOOL WINAPI spn_windows_console_handler(DWORD ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
    sp_atomic_s32_set(&spn.sp->shutdown, 1);
    return TRUE;
  }
  return FALSE;
}

void spn_install_signal_handlers() {
  SetConsoleCtrlHandler(spn_windows_console_handler, TRUE);
}
#endif
