#ifndef SMOKE_DOCKER_H
#define SMOKE_DOCKER_H

#include "sp.h"
#include "sp_template/sp_template.h"
#include "toolchain/types.h"

#include "lanes.h"
#include "variant/variant.h"

typedef enum {
  DOCKER_INIT_OK,
  DOCKER_INIT_ERR_REPO,
  DOCKER_INIT_ERR_BINARY,
  DOCKER_INIT_ERR_TEMPLATES,
  DOCKER_INIT_ERR_LANES,
  DOCKER_INIT_ERR_VERIFY,
  DOCKER_INIT_ERR_CONFIG,
} docker_init_err_t;

typedef enum {
  DOCKER_TESTS_OK,
  DOCKER_TESTS_ERR_GIT,
  DOCKER_TESTS_ERR_USER,
  DOCKER_TESTS_ERR_BINARY,
} docker_tests_err_t;

typedef enum {
  DOCKER_PROVISION_OK,
  DOCKER_PROVISION_ERR_ARTIFACT,
  DOCKER_PROVISION_ERR_FETCH,
  DOCKER_PROVISION_ERR_XWIN,
} docker_provision_err_t;

typedef enum {
  DOCKER_RENDER_OK,
  DOCKER_RENDER_ERR_MISSING,
  DOCKER_RENDER_ERR_FAILED,
} docker_render_err_t;

typedef struct {
  sp_mem_t mem;
  sp_template_registry_t* templates;
  lanes_t builtin;
  lanes_t lanes;
  sp_str_t issues [LANE_COUNT];
  spn_toolchain_catalog_t catalog;
  spn_toolchain_store_t store;
  sp_str_t host;
  struct {
    sp_str_t repo;
    sp_str_t git;
    sp_str_t tools;
    sp_str_t tests;
    sp_str_t zig;
    sp_str_t home;
    sp_str_t dockerfiles;
    sp_str_t templates;
    sp_str_t builtin;
    sp_str_t lanes;
    sp_str_t config;
    sp_str_t logs;
    struct {
      sp_str_t cache;
      sp_str_t splat;
    } xwin;
  } paths;
  const c8* user;
  struct {
    struct {
      sp_str_t path;
      const c8* hint;
    } binary;
    struct {
      const c8* name;
      sp_str_t url;
    } artifact;
    struct {
      sp_str_t output;
      s32 status;
    } xwin;
    struct {
      sp_str_t path;
      sp_str_t issues;
    } lanes;
    verify_t verify;
    s32 render;
  } err;
} docker_t;

docker_init_err_t      docker_init(docker_t* docker, sp_mem_t mem, spn_fetch_fn fetch, void* user);
docker_tests_err_t     docker_tests_init(docker_t* docker);
bool                   docker_require(docker_t* docker, const variant_t* variant);
docker_provision_err_t docker_provision(docker_t* docker, const variant_t* variant);
docker_render_err_t    docker_render(docker_t* docker, const variant_t* variant);
const c8*              docker_image(docker_t* docker, const variant_t* variant);
sp_ps_config_t         docker_build(docker_t* docker, const variant_t* variant);
sp_ps_config_t         docker_check(docker_t* docker, const variant_t* variant);
sp_ps_config_t         docker_shell(docker_t* docker, const variant_t* variant);
sp_ps_config_t         docker_test(docker_t* docker, const variant_t* variant, lane_t lane, const c8* filter);
sp_str_t               docker_log(docker_t* docker, const c8* name);
sp_str_t               docker_lane_issues(docker_t* docker, lane_t lane);

#endif
