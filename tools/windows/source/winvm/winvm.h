#ifndef WINVM_WINVM_H
#define WINVM_WINVM_H

#include "sp.h"
#include "sp_template/sp_template.h"

#include "variant/variant.h"

typedef enum {
  WINVM_INIT_OK,
  WINVM_INIT_ERR_NO_DIR,
  WINVM_INIT_ERR_TEMPLATES,
} winvm_init_err_t;

typedef struct {
  sp_mem_t mem;
  sp_template_registry_t* templates;

  struct {
    sp_str_t repo;
    sp_str_t dir;
    sp_str_t golden;
    sp_str_t recipes;
    sp_str_t templates;
    sp_str_t domains;
    sp_str_t logs;
    sp_str_t known_hosts;
  } paths;

  struct {
    sp_str_t connect;
    sp_str_t pool;
    sp_str_t origin;
    sp_str_t snapshot;
    sp_str_t network;
    sp_str_t prefix;
    sp_str_t user;
  } cfg;
} winvm_t;

winvm_init_err_t winvm_init(winvm_t* vm, sp_mem_t mem);

sp_str_t winvm_image(winvm_t* vm, const winvm_variant_t* variant);
sp_str_t winvm_work(winvm_t* vm, const winvm_variant_t* variant);
sp_str_t winvm_domain(winvm_t* vm, const winvm_variant_t* variant);
sp_str_t winvm_ip(winvm_t* vm, const winvm_variant_t* variant);
sp_str_t winvm_log(winvm_t* vm, const c8* name);
sp_str_t winvm_state(winvm_t* vm, const winvm_variant_t* variant);
bool     winvm_running(winvm_t* vm, const winvm_variant_t* variant);

s32 winvm_overlay(winvm_t* vm, sp_str_t backing, sp_str_t path);
s32 winvm_lease(winvm_t* vm, const winvm_variant_t* variant);
s32 winvm_boot(winvm_t* vm, const winvm_variant_t* variant, sp_str_t disk);
s32 winvm_destroy(winvm_t* vm, const winvm_variant_t* variant);
s32 winvm_undefine(winvm_t* vm, const winvm_variant_t* variant);
s32 winvm_seal(winvm_t* vm, sp_str_t path);
s32 winvm_shutdown(winvm_t* vm, const winvm_variant_t* variant);
s32 winvm_upload_recipe(winvm_t* vm, const winvm_variant_t* variant, winvm_step_t step);
s32 winvm_upload_file(winvm_t* vm, const winvm_variant_t* variant, sp_str_t local, sp_str_t remote);

sp_ps_config_t winvm_recipe_config(winvm_t* vm, const winvm_variant_t* variant, winvm_step_t step);
sp_ps_config_t winvm_wait_ssh_config(winvm_t* vm, const winvm_variant_t* variant, u32 timeout_s);
sp_ps_config_t winvm_wait_off_config(winvm_t* vm, const winvm_variant_t* variant, u32 timeout_s);
sp_ps_config_t winvm_voldownload_config(winvm_t* vm, sp_str_t dest);
sp_ps_config_t winvm_convert_config(winvm_t* vm, sp_str_t src, sp_str_t dest);
sp_ps_config_t winvm_ssh_config(winvm_t* vm, const winvm_variant_t* variant, const c8* command);

#endif
