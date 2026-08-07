#ifndef SPN_SESSION_H
#define SPN_SESSION_H

#include "sp.h"
#include "core.h"

typedef enum {
  SPN_INDEX_KIND_WORKSPACE,
  SPN_INDEX_KIND_BUILTIN,
  SPN_INDEX_KIND_USER,
} spn_index_kind_t;

typedef enum {
  SPN_INDEX_PROTOCOL_GIT,
  SPN_INDEX_PROTOCOL_HTTP,
  SPN_INDEX_PROTOCOL_DIR,
} spn_index_protocol_t;

typedef sp_da(sp_str_t) spn_target_names_t;

typedef enum {
  SPN_TARGET_RULE_NONE,
  SPN_TARGET_RULE_ALL,
  SPN_TARGET_RULE_NAMED,
} spn_target_rule_kind_t;

typedef struct {
  spn_target_rule_kind_t kind;
  spn_target_names_t names;
} spn_target_rule_t;

typedef struct {
  spn_target_rule_t bin;
  spn_target_rule_t lib;
  spn_target_rule_t test;
  spn_target_rule_t script;
} spn_target_selection_t;

typedef struct {
  sp_str_t name;
  sp_str_t toolchain;
  spn_build_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
  bool sanitizers_set;
  spn_triple_t triple;
} spn_profile_override_t;

typedef struct spn_session_config_t {
  spn_target_selection_t selection;
  spn_profile_override_t profile;
  bool force;
} spn_session_config_t;

#endif
