#include "host/host.h"

#include "install/install.h"
#include "tui/tui.h"
#include "version.h"

static void print_msg(spn_install_msg_t* msg) {
  switch (msg->kind) {
    case SPN_INSTALL_MSG_NONE: {
      break;
    }
    case SPN_INSTALL_MSG_INSTALLED: {
      spn_print(&tui, "install: spn {} installed to {}", sp_fmt_cstr(SPN_VERSION), sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_RESTART_SHELL: {
      spn_print(&tui, "install: restart your shell, or run:");
      spn_print(&tui, "install:   {}", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_RESTART_TERMINAL: {
      spn_print(&tui, "install: restart your terminal to use spn");
      break;
    }
    case SPN_INSTALL_MSG_MANUAL: {
      spn_print(&tui, "install: add {} to your PATH", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_STUCK_FILE: {
      spn_print_err(&tui, "install: could not update {}", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_STUCK_REGISTRY: {
      spn_print_err(&tui, "install: could not update the user PATH in the registry");
      break;
    }
    case SPN_INSTALL_MSG_ADD_LINE: {
      spn_print_err(&tui, "install: add this line to your shell profile:");
      spn_print_err(&tui, "install:   {}", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_SHADOW: {
      spn_print_err(&tui, "install: another spn at {} shadows {}", sp_fmt_str(msg->subject), sp_fmt_str(msg->detail));
      break;
    }
  }
}

static sp_cli_result_t install(sp_cli_t* cli) {
  spn_tui_handoff(&tui);

  sp_mem_t mem = host.mem;
  sp_env_t env = sp_env_capture(mem);
  spn_install_layout_t layout = spn_install_resolve(mem, spn_install_os_host(), &env);
  switch (layout.err) {
    case SPN_INSTALL_OK: break;
    case SPN_INSTALL_ERR_NO_HOME: return spn_cli_error(cli, "HOME is not set; set SPN_INSTALL to choose an install directory");
    case SPN_INSTALL_ERR_ROOT_CHARS: return spn_cli_error(cli, "SPN_INSTALL may not contain a quote, dollar sign, backtick, or backslash");
  }

  spn_install_facts_t facts = spn_install_probe(mem, &layout);
  spn_install_plan_t plan = spn_install_plan(mem, &layout, &facts);
  spn_install_result_t result = spn_install_execute(&plan);
  spn_install_msgs_t msgs = spn_install_report(&layout, &facts, &plan, &result);
  sp_for(it, msgs.count) {
    print_msg(&msgs.items[it]);
  }

  if (result.err) {
    switch (result.failed.kind) {
      case SPN_INSTALL_ACTION_INSTALL_EXE: {
        return spn_cli_error(cli, "failed to replace {}; close any running spn and retry", sp_fmt_str(result.failed.path));
      }
      default: {
        return spn_cli_error(cli, "failed to create {}", sp_fmt_str(result.failed.path));
      }
    }
  }
  return SP_CLI_OK;
}

static sp_cli_cmd_t spn_cmd_self_install = {
  .name = "install",
  .summary = "Install this binary and set up your PATH",
  .handler = install,
};

sp_cli_cmd_t spn_cmd_self = {
  .name = "self",
  .summary = "Manage this spn installation",
  .commands = {
    &spn_cmd_self_install,
  },
};
