#ifndef SMOKE_DOCKER_H
#define SMOKE_DOCKER_H

#include "sp.h"

#include "variant/variant.h"

typedef struct sp_template_registry sp_template_registry_t;

typedef enum {
  DOCKER_INIT_OK,
  DOCKER_INIT_ERR_REPO,
  DOCKER_INIT_ERR_BINARY,
  DOCKER_INIT_ERR_TEMPLATES,
} docker_init_err_t;

typedef enum {
  DOCKER_TESTS_OK,
  DOCKER_TESTS_ERR_GIT,
  DOCKER_TESTS_ERR_USER,
  DOCKER_TESTS_ERR_BINARY,
} docker_tests_err_t;

typedef enum {
  DOCKER_RENDER_OK,
  DOCKER_RENDER_ERR_MISSING,
  DOCKER_RENDER_ERR_FAILED,
} docker_render_err_t;

typedef struct {
  sp_mem_t mem;
  sp_template_registry_t* templates;
  struct {
    sp_str_t repo;
    sp_str_t git;
    sp_str_t tools;
    sp_str_t tests;
    sp_str_t toolchains;
    sp_str_t zig;
    sp_str_t home;
    sp_str_t dockerfiles;
    sp_str_t templates;
  } paths;
  const c8* user;
  struct {
    struct {
      sp_str_t path;
      const c8* hint;
    } binary;
    s32 render;
  } err;
} docker_t;

docker_init_err_t docker_init(docker_t* docker, sp_mem_t mem);
docker_tests_err_t docker_tests_init(docker_t* docker);
bool docker_require(docker_t* docker, const variant_t* variant);
docker_render_err_t docker_render(docker_t* docker, const variant_t* variant);
const c8* docker_image(docker_t* docker, const variant_t* variant);
sp_ps_config_cstr_t docker_build(docker_t* docker, const variant_t* variant);
sp_ps_config_cstr_t docker_check(docker_t* docker, const variant_t* variant);
sp_ps_config_cstr_t docker_shell(docker_t* docker, const variant_t* variant);
sp_ps_config_cstr_t docker_test(docker_t* docker, const variant_t* variant, const c8* lane, const c8* filter);

#endif
