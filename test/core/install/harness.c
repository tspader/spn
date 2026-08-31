#include "harness.h"

sp_env_t install_env(sp_mem_t mem, const install_var_t* vars) {
  sp_env_t env = sp_zero;
  sp_env_init(mem, &env);
  sp_for(it, INSTALL_MAX_VARS) {
    if (!vars[it].name) {
      break;
    }
    sp_env_insert(&env, sp_cstr_as_str(vars[it].name), sp_cstr_as_str(vars[it].value));
  }
  return env;
}

sp_err_t install_build(sp_test_t* t, install_world_t* world, spn_install_layout_t* layout, spn_install_facts_t* facts) {
  sp_must(t, world->vars[0].name);
  sp_must(t, world->exe);

  sp_mem_t mem = sp_test_arena(t);
  sp_env_t env = install_env(mem, world->vars);

  *layout = spn_install_resolve(mem, world->os, &env);
  sp_must_eq(t, (u32)SPN_INSTALL_OK, (u32)layout->err);

  *facts = sp_zero_s(spn_install_facts_t);
  facts->exe = sp_cstr_as_str(world->exe);
  facts->shadow = world->shadow ? sp_cstr_as_str(world->shadow) : sp_zero_s(sp_str_t);
  sp_carr_for(world->rc, it) {
    facts->rc[it] = world->rc[it];
  }
  facts->registry.path = world->registry.path ? sp_cstr_as_str(world->registry.path) : sp_zero_s(sp_str_t);
  facts->registry.kind = world->registry.kind;
  return SP_OK;
}

sp_err_t install_expect_actions(sp_test_t* t, const spn_install_action_t* actual, u32 count, const install_action_spec_t* expect, u32 num_expect) {
  sp_must_eq(t, num_expect, count);

  sp_for(it, count) {
    sp_expect_eq(t, (u32)expect[it].kind, (u32)actual[it].kind);
    sp_expect_str_eq_c(t, actual[it].path, expect[it].path ? expect[it].path : "");
    sp_expect_str_eq_c(t, actual[it].src, expect[it].src ? expect[it].src : "");
    sp_expect_str_eq_c(t, actual[it].text, expect[it].text ? expect[it].text : "");
    sp_expect_eq(t, (u32)expect[it].reg, (u32)actual[it].reg);
    sp_expect_eq(t, (u32)expect[it].role, (u32)actual[it].role);
  }
  return SP_OK;
}
