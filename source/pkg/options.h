#ifndef SPN_PKG_OPTIONS_H
#define SPN_PKG_OPTIONS_H

#include "forward/types.h"
#include "pkg/types.h"
#include "profile/types.h"
#include "when/types.h"

typedef struct {
  sp_str_t consumer;
  const spn_when_t* options;
} spn_option_request_t;

typedef sp_da(spn_option_request_t) spn_option_requests_t;
typedef sp_ht(sp_intern_id_t, spn_option_requests_t) spn_option_seeds_t;

typedef struct {
  sp_str_t name;
  spn_option_value_t value;
  bool is_default;
} spn_resolved_option_t;

typedef sp_da(spn_resolved_option_t) spn_resolved_options_t;

spn_pkg_config_t* spn_pkg_config_find(sp_da(spn_pkg_config_entry_t) config, sp_str_t name);

void spn_when_env_from_profile(sp_mem_t mem, const spn_profile_info_t* profile, spn_when_env_t* env);
void spn_when_env_add_options(spn_when_env_t* env, const spn_resolved_options_t* options);

spn_err_t spn_pkg_options_merge(
  sp_mem_t mem,
  const spn_resolved_pkg_t* pkg,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  spn_option_requests_t requests,
  spn_event_buffer_t* events,
  spn_resolved_options_t* resolved);

void spn_pkg_options_env(
  sp_mem_t mem,
  const spn_resolved_pkg_t* pkg,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  spn_option_requests_t requests,
  spn_when_env_t* env);

void spn_pkg_apply_options(spn_pkg_info_t* info, spn_when_env_t* env);

#endif
