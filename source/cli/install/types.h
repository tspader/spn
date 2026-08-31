#ifndef SPN_INSTALL_TYPES_H
#define SPN_INSTALL_TYPES_H

#include "sp.h"

#define SPN_INSTALL_MAX_RC 6
#define SPN_INSTALL_MAX_PATH_CHOICES 4
#define SPN_INSTALL_MAX_ACTIONS 12
#define SPN_INSTALL_MAX_MSGS 12

#define SPN_INSTALL_ROOT_EXPR "$HOME/.spn"
#define SPN_INSTALL_RC_LINE ". \"" SPN_INSTALL_ROOT_EXPR "/env\""

#define SPN_INSTALL_ENV_SH \
  "case \":${PATH}:\" in\n" \
  "  *:\"" SPN_INSTALL_ROOT_EXPR "/bin\":*) ;;\n" \
  "  *) export PATH=\"" SPN_INSTALL_ROOT_EXPR "/bin:${PATH}\" ;;\n" \
  "esac\n"

#define SPN_INSTALL_FISH_SH \
  "if not contains \"" SPN_INSTALL_ROOT_EXPR "/bin\" $PATH\n" \
  "  set --export PATH \"" SPN_INSTALL_ROOT_EXPR "/bin\" $PATH\n" \
  "end\n"

typedef enum {
  SPN_INSTALL_OS_UNIX,
  SPN_INSTALL_OS_WINDOWS,
} spn_install_os_t;

typedef enum {
  SPN_INSTALL_OK = 0,
  SPN_INSTALL_ERR_NO_HOME,
  SPN_INSTALL_ERR_EXE,
  SPN_INSTALL_ERR_STUCK,
} spn_install_err_t;

typedef enum {
  SPN_INSTALL_REG_NONE = 0,
  SPN_INSTALL_REG_SZ,
  SPN_INSTALL_REG_EXPAND,
  SPN_INSTALL_REG_OTHER,
} spn_install_reg_t;

typedef enum {
  SPN_INSTALL_SHELL_NONE = 0,
  SPN_INSTALL_SHELL_BASH,
  SPN_INSTALL_SHELL_ZSH,
  SPN_INSTALL_SHELL_FISH,
  SPN_INSTALL_SHELL_CUSTOM,
} spn_install_shell_t;

typedef enum {
  // hook it only if the user already has one
  SPN_INSTALL_RC_HOOK_EXISTING = 0,
  // hook it, creating it if missing
  SPN_INSTALL_RC_HOOK_ALWAYS,
  // evidence its shell is in use; never written
  SPN_INSTALL_RC_PROBE,
} spn_install_rc_role_t;

typedef struct {
  sp_str_t path;
  spn_install_rc_role_t role;
  spn_install_shell_t shell;
} spn_install_rc_t;

typedef struct {
  spn_install_err_t err;
  spn_install_os_t os;
  sp_str_t home;
  sp_str_t bin;
  sp_str_t bin_native;
  sp_str_t exe;
  sp_str_t env_file;
  sp_str_t fish_dir;
  sp_str_t fish_conf;
  // the login shell from $SHELL; evidence a shell is in use even when it has
  // no config files yet, which is exactly a fresh macos account
  spn_install_shell_t login;
  spn_install_rc_t rc [SPN_INSTALL_MAX_RC];
  u32 num_rc;
  sp_da(sp_str_t) shadows;
  sp_str_t github_path;
  bool no_modify_path;
  bool on_path;
} spn_install_layout_t;

typedef struct {
  bool exists;
  bool has_line;
} spn_install_rc_state_t;

typedef struct {
  spn_install_shell_t kind;
  sp_str_t custom;
  bool has_line;
} spn_install_path_choice_t;

typedef struct {
  spn_install_path_choice_t path [SPN_INSTALL_MAX_PATH_CHOICES];
  u32 num_path;
  bool registry;
} spn_install_choices_t;

typedef struct {
  sp_str_t exe;
  sp_str_t shadow;
  // the fish config dir exists; evidence fish is in use
  bool fish;
  // the env file already holds exactly what we would write
  bool env_current;
  // the fish conf already holds exactly what we would write
  bool fish_current;
  spn_install_rc_state_t rc [SPN_INSTALL_MAX_RC];
  struct {
    sp_str_t path;
    spn_install_reg_t kind;
  } registry;
} spn_install_facts_t;

typedef struct {
  spn_install_layout_t layout;
  spn_install_facts_t facts;
} spn_install_probe_t;

typedef enum {
  SPN_INSTALL_ACTION_NONE = 0,
  SPN_INSTALL_ACTION_INSTALL_EXE,
  SPN_INSTALL_ACTION_WRITE_FILE,
  SPN_INSTALL_ACTION_APPEND_LINE,
  SPN_INSTALL_ACTION_SET_USER_PATH,
} spn_install_action_kind_t;

typedef enum {
  SPN_INSTALL_ROLE_NONE = 0,
  SPN_INSTALL_ROLE_EXE,
  SPN_INSTALL_ROLE_ENV,
  SPN_INSTALL_ROLE_HOOK,
} spn_install_role_t;

typedef struct {
  spn_install_action_kind_t kind;
  spn_install_role_t role;
  sp_str_t path;
  sp_str_t src;
  sp_str_t text;
  sp_str_t line;
  spn_install_reg_t reg;
} spn_install_action_t;

typedef enum {
  SPN_INSTALL_PATH_OK = 0,
  SPN_INSTALL_PATH_UPDATED,
  SPN_INSTALL_PATH_CI,
  SPN_INSTALL_PATH_MANUAL,
} spn_install_path_state_t;

typedef struct {
  spn_install_action_t actions [SPN_INSTALL_MAX_ACTIONS];
  u32 count;
  spn_install_path_state_t state;
  // hooks that already carry us and so are not in the plan
  u32 live;
  // at least one hook, planned or live, is read by a posix shell
  bool posix;
} spn_install_plan_t;

typedef struct {
  sp_err_t err;
  spn_install_action_t failed;
  u32 stuck [SPN_INSTALL_MAX_ACTIONS];
  u32 num_stuck;
} spn_install_result_t;

typedef enum {
  SPN_INSTALL_MSG_NONE = 0,
  SPN_INSTALL_MSG_STUCK_WRITE,
  SPN_INSTALL_MSG_STUCK_APPEND,
  SPN_INSTALL_MSG_STUCK_REGISTRY,
  SPN_INSTALL_MSG_RESTART_SHELL,
  SPN_INSTALL_MSG_RESTART_TERMINAL,
  SPN_INSTALL_MSG_MANUAL,
  SPN_INSTALL_MSG_SHADOW,
} spn_install_msg_kind_t;

typedef struct {
  spn_install_msg_kind_t kind;
  sp_str_t subject;
  sp_str_t detail;
} spn_install_msg_t;

typedef struct {
  spn_install_msg_t items [SPN_INSTALL_MAX_MSGS];
  u32 count;
} spn_install_msgs_t;

typedef struct {
  spn_install_err_t err;
  spn_install_action_t failed;
  sp_str_t exe;
  u32 changes;
  u32 stuck;
  spn_install_msgs_t msgs;
} spn_install_t;

#endif
