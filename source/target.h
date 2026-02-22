#ifndef SPN_TARGET_H
#define SPN_TARGET_H

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

struct spn_target {
  sp_str_t name;
  spn_target_kind_t kind;
  spn_pkg_t* pkg;
  spn_visibility_t visibility;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(spn_embed_t) embed;
};

spn_target_kind_t spn_pkg_linkage_to_target_kind(spn_linkage_t kind);
spn_linkage_t spn_target_kind_to_pkg_linkage(spn_target_kind_t kind);

#endif
