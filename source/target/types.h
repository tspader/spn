#ifndef SPN_TARGET_TYPES_H
#define SPN_TARGET_TYPES_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"
#include "compiler/types.h"
#include "when/types.h"

#define SP_EMBED_DEFAULT_SYMBOL_S sp_str_lit("")
#define SP_EMBED_DEFAULT_DATA_T_S sp_str_lit("")
#define SP_EMBED_DEFAULT_SIZE_T_S sp_str_lit("")

typedef enum {
  SPN_EMBED_FILE,
  SPN_EMBED_MEM,
  SPN_EMBED_DIR,
} spn_embed_kind_t;

typedef struct {
  sp_str_t data;
  sp_str_t size;
} spn_embed_types_t;

typedef struct {
  spn_embed_kind_t kind;
  sp_str_t symbol;
  spn_embed_types_t types;
  union {
    struct { sp_str_t path; } file;
    struct { const u8* buffer; u64 size; } memory;
    struct { sp_str_t path; } dir;
  };
} spn_embed_t;


typedef enum {
  SPN_TARGET_LIB,
  SPN_TARGET_EXE,
  SPN_TARGET_SCRIPT,
  SPN_TARGET_TEST,
} spn_target_kind_t;

typedef struct {
  bool source;
  bool shared;
  bool static_lib;
  bool object;
} spn_linkage_set_t;

typedef sp_opt(spn_linkage_t) sp_opt_spn_linkage_t;

struct spn_target_info {
  sp_str_t name;
  spn_target_kind_t kind;
  spn_linkage_set_t linkages;
  // An unlinked lib still builds and installs, but consumers don't link it;
  // think runtime data that happens to be an archive (e.g. tcc's libtcc1.a)
  bool no_link;
  sp_da(sp_str_t) source;
  sp_da(sp_str_t) headers;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) flags;
  sp_da(sp_str_t) system_deps;
  sp_da(sp_str_t) deps;
  sp_da(spn_embed_t) embed;
  spn_cxx_options_t cxx;
  // Manifest entries land here at load; spn_pkg_apply_options folds the ones
  // whose predicates pass into the plain lists above, which stay empty until
  // then. Script-created targets skip this and write the plain lists directly.
  struct {
    spn_gated_list_t source;
    spn_gated_list_t define;
    spn_gated_list_t flags;
    spn_gated_list_t system_deps;
    spn_gated_list_t deps;
  } gated;
};

#endif
