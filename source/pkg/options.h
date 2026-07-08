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

typedef struct {
  sp_str_t name;
  spn_option_value_t value;
  bool is_default;
} spn_resolved_option_t;

typedef sp_da(spn_resolved_option_t) spn_resolved_options_t;

spn_pkg_config_t* spn_pkg_config_find(sp_da(spn_pkg_config_entry_t) config, sp_str_t name);

void spn_when_env_from_profile(sp_mem_t mem, const spn_profile_info_t* profile, spn_when_env_t* env);
void spn_when_env_add_options(spn_when_env_t* env, const spn_resolved_options_t* options);

// One resolved value per declared option, in declaration order. Root config
// sets authoritatively (the root's own options come from the profile instead),
// additive bools union across requests, everything else must agree across
// requests with the root as tiebreaker, { not = v } requests veto the winner,
// and unset options fall through to their first-match defaults unless the
// root declined them. With events NULL nothing is validated or reported; the
// resolver uses that silent form to gate dep edges before requests exist.
spn_err_t spn_pkg_options_merge(
  sp_mem_t mem,
  spn_pkg_info_t* info,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  bool is_root,
  sp_da(spn_option_request_t) requests,
  spn_event_buffer_t* events,
  spn_resolved_options_t* out);

void spn_pkg_options_env(
  sp_mem_t mem,
  spn_pkg_info_t* info,
  const spn_profile_info_t* profile,
  sp_da(spn_pkg_config_entry_t) root_config,
  bool is_root,
  spn_when_env_t* env);

// The transform from manifest-shaped data to build-shaped data: every gated
// list folds into the plain list beside it under env, and each true bool
// option with a define lands in the package's define list (public ones also
// in public_define, which direct consumers compile with). Idempotent because
// file-backed packages share one info across resolved instances.
void spn_pkg_apply_options(spn_pkg_info_t* info, spn_when_env_t* env);

#endif
