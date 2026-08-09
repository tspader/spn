#ifndef SPN_TYPES_H
#define SPN_TYPES_H

#include "sp.h"
#include "core.h"

typedef struct {
  sp_str_t* items;
  u32 count;
} spn_str_arr_t;

SP_TYPEDEF_FN(void, spn_wake_fn_t, void*);

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

typedef struct {
  sp_str_t name;
  spn_index_kind_t kind;
  spn_index_protocol_t protocol;
  sp_str_t source;
  sp_str_t location;
} spn_index_desc_t;

typedef struct {
  spn_index_desc_t* items;
  u32 count;
} spn_index_arr_t;

typedef enum {
  SPN_TARGET_RULE_NONE,
  SPN_TARGET_RULE_ALL,
  SPN_TARGET_RULE_NAMED,
} spn_target_rule_kind_t;

typedef struct {
  spn_target_rule_kind_t kind;
  spn_str_arr_t names;
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

typedef enum {
  SPN_OP_ADD,
  SPN_OP_PUBLISH,
  SPN_OP_SYNC_INDEXES,
  SPN_OP_SCAFFOLD,
  SPN_OP_BUILD,
  SPN_OP_TEST,
  SPN_OP_RUN_TARGET,
  SPN_OP_CLEAN,
  SPN_OP_CLEAN_PROFILE,
} spn_op_kind_t;

typedef struct {
  u32 total;
  u32 completed;
  u32 hits;
  u32 misses;
} spn_progress_t;

typedef struct {
  sp_str_t dir;
  u32 index_refresh_seconds;
  bool project_optional;
} spn_open_request_t;

typedef struct {
  sp_str_t name;
  sp_str_t version;
  spn_dep_kind_t kind;
} spn_add_request_t;

typedef struct {
  bool force;
  sp_str_t only;
} spn_sync_request_t;

typedef struct {
  sp_str_t index;
  sp_str_t url;
  sp_str_t revision;
  bool allow_dirty;
  bool dry;
} spn_publish_request_t;

typedef struct {
  sp_str_t dir;
  sp_str_t name;
  bool bare;
} spn_scaffold_request_t;

typedef struct {
  spn_str_arr_t files;
} spn_scaffold_result_t;

typedef struct {
  u32 passed;
  u32 failed;
} spn_test_result_t;

typedef struct {
  sp_str_t json;
} spn_publish_result_t;

typedef struct {
  spn_op_kind_t kind;
  spn_err_t err;
  union {
    spn_scaffold_result_t scaffold;
    spn_test_result_t test;
    spn_publish_result_t publish;
  };
} spn_op_result_t;

#endif
