#ifndef SPN_TARGET_TYPES_H
#define SPN_TARGET_TYPES_H

#include "sp.h"
#include "spn.h"

#include "embed.h"

typedef enum {
  SPN_TARGET_NONE,
  SPN_TARGET_SHARED_LIB,
  SPN_TARGET_STATIC_LIB,
  SPN_TARGET_EXE,
  SPN_TARGET_JIT,
  SPN_TARGET_OBJECT,
} spn_target_kind_t;

typedef struct {
  bool source;
  bool shared;
  bool static_lib;
} spn_linkage_set_t;

struct spn_target {
  sp_str_t name;
  spn_target_kind_t kind;
  spn_linkage_set_t linkages;
  spn_pkg_t* pkg;
  spn_visibility_t visibility;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(spn_embed_t) embed;
};

#endif
