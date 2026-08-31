#ifndef SPN_INSTALL_TEST_HARNESS_H
#define SPN_INSTALL_TEST_HARNESS_H

#include "spn_test.h"
#include "install/plan.h"

#define INSTALL_MAX_VARS 6

#define INSTALL_WORLD_UNIX \
  .vars = { { "HOME", "/h" }, { "PATH", "/p" } }, \
  .exe = "/s/spn"

#define INSTALL_WORLD_WINDOWS \
  .vars = { { "USERPROFILE", "C:\\u" }, { "PATH", "C:\\p" } }, \
  .os = SPN_INSTALL_OS_WINDOWS, \
  .exe = "C:/s/spn.exe"

enum {
  INSTALL_RC_PROFILE,
  INSTALL_RC_BASHRC,
  INSTALL_RC_BASH_PROFILE,
  INSTALL_RC_BASH_LOGIN,
  INSTALL_RC_ZSHENV,
  INSTALL_RC_ZSHRC,
};

typedef struct {
  const c8* name;
  const c8* value;
} install_var_t;

typedef struct {
  spn_install_action_kind_t kind;
  spn_install_role_t role;
  const c8* path;
  const c8* src;
  const c8* text;
  const c8* line;
  spn_install_reg_t reg;
} install_action_spec_t;

typedef struct {
  install_var_t vars [INSTALL_MAX_VARS];
  spn_install_os_t os;
  const c8* exe;
  const c8* shadow;
  bool fish;
  bool env_current;
  bool fish_current;
  spn_install_rc_state_t rc [SPN_INSTALL_MAX_RC];
  struct {
    const c8* path;
    spn_install_reg_t kind;
  } registry;
} install_world_t;

sp_env_t install_env(sp_mem_t mem, const install_var_t* vars);
sp_err_t install_build(sp_test_t* t, install_world_t* world, spn_install_layout_t* layout, spn_install_facts_t* facts);
sp_err_t install_expect_actions(sp_test_t* t, const spn_install_action_t* actual, u32 count, const install_action_spec_t* expect, u32 num_expect);

#define install_expect_action_arr(t, actual, count, arr) \
  do { \
    u32 __num_expect = 0; \
    sp_carr_detect_len(arr, __num_expect, (arr)[__num_expect].kind); \
    sp_try(install_expect_actions(t, actual, count, arr, __num_expect)); \
  } while (0)

#endif
