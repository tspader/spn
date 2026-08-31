#ifndef SPN_INSTALL_TYPES_H
#define SPN_INSTALL_TYPES_H

#include "sp.h"

#define SPN_INSTALL_MAX_RC 5
#define SPN_INSTALL_MAX_INSTALL_ACTIONS 2
#define SPN_INSTALL_MAX_PATH_ACTIONS 8
#define SPN_INSTALL_MAX_MSGS 12

typedef enum {
  SPN_INSTALL_OS_UNIX,
  SPN_INSTALL_OS_WINDOWS,
} spn_install_os_t;

typedef enum {
  SPN_INSTALL_OK = 0,
  SPN_INSTALL_ERR_NO_HOME,
  SPN_INSTALL_ERR_ROOT_CHARS,
} spn_install_err_t;

typedef enum {
  SPN_INSTALL_REG_NONE = 0,
  SPN_INSTALL_REG_SZ,
  SPN_INSTALL_REG_EXPAND,
  SPN_INSTALL_REG_OTHER,
} spn_install_reg_t;

typedef struct {
  sp_str_t path;
  bool always;
} spn_install_rc_t;

typedef struct {
  spn_install_err_t err;
  spn_install_os_t os;
  sp_str_t root;
  sp_str_t root_expr;
  sp_str_t bin;
  sp_str_t bin_native;
  sp_str_t exe;
  sp_str_t env_file;
  sp_str_t rc_line;
  sp_str_t fish_conf;
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
  sp_str_t exe;
  sp_str_t shadow;
  spn_install_rc_state_t rc [SPN_INSTALL_MAX_RC];
  struct {
    sp_str_t path;
    spn_install_reg_t kind;
  } registry;
} spn_install_facts_t;

typedef enum {
  SPN_INSTALL_ACTION_NONE = 0,
  SPN_INSTALL_ACTION_CREATE_DIR,
  SPN_INSTALL_ACTION_INSTALL_EXE,
  SPN_INSTALL_ACTION_WRITE_FILE,
  SPN_INSTALL_ACTION_APPEND_LINE,
  SPN_INSTALL_ACTION_SET_USER_PATH,
} spn_install_action_kind_t;

typedef enum {
  SPN_INSTALL_ROLE_NONE = 0,
  SPN_INSTALL_ROLE_PATH,
  SPN_INSTALL_ROLE_RC,
  SPN_INSTALL_ROLE_FISH,
} spn_install_role_t;

typedef struct {
  spn_install_action_kind_t kind;
  sp_str_t path;
  sp_str_t src;
  sp_str_t text;
  spn_install_reg_t reg;
  spn_install_role_t role;
} spn_install_action_t;

typedef enum {
  SPN_INSTALL_PATH_OK = 0,
  SPN_INSTALL_PATH_UPDATED,
  SPN_INSTALL_PATH_CI,
  SPN_INSTALL_PATH_MANUAL,
} spn_install_path_state_t;

typedef struct {
  spn_install_action_t install [SPN_INSTALL_MAX_INSTALL_ACTIONS];
  u32 num_install;
  spn_install_action_t path [SPN_INSTALL_MAX_PATH_ACTIONS];
  u32 num_path;
  spn_install_path_state_t state;
} spn_install_plan_t;

typedef struct {
  sp_err_t err;
  spn_install_action_t failed;
  u32 stuck [SPN_INSTALL_MAX_PATH_ACTIONS];
  u32 num_stuck;
} spn_install_result_t;

typedef enum {
  SPN_INSTALL_MSG_NONE = 0,
  SPN_INSTALL_MSG_INSTALLED,
  SPN_INSTALL_MSG_RESTART_SHELL,
  SPN_INSTALL_MSG_RESTART_TERMINAL,
  SPN_INSTALL_MSG_MANUAL,
  SPN_INSTALL_MSG_STUCK_FILE,
  SPN_INSTALL_MSG_STUCK_REGISTRY,
  SPN_INSTALL_MSG_ADD_LINE,
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

#endif
