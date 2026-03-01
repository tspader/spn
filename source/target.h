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

typedef struct {
  bool source;
  bool shared;
  bool static_lib;
} spn_linkage_set_t;

sp_str_t spn_visibility_to_str(spn_visibility_t kind);
spn_visibility_t spn_visibility_from_str(sp_str_t str);
spn_linkage_t spn_lib_kind_from_str(sp_str_t str);
spn_linkage_t spn_pkg_linkage_from_str(sp_str_t str);
sp_str_t spn_pkg_linkage_to_str(spn_linkage_t kind);
sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_linkage_t kind);
void spn_linkage_set_add(spn_linkage_set_t* set, spn_linkage_t kind);
bool spn_linkage_set_has(spn_linkage_set_t set, spn_linkage_t kind);
spn_linkage_t spn_linkage_set_default(spn_linkage_set_t set);

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

spn_target_kind_t spn_pkg_linkage_to_target_kind(spn_linkage_t kind);
spn_linkage_t spn_target_kind_to_pkg_linkage(spn_target_kind_t kind);

void spn_target_add_source(spn_target_t* target, const c8* source);
void spn_target_add_source_ex(spn_target_t* target, sp_str_t source);
void spn_target_add_include(spn_target_t* target, const c8* include);
void spn_target_add_include_ex(spn_target_t* target, sp_str_t include);
void spn_target_add_define(spn_target_t* target, const c8* define);
void spn_target_add_define_ex(spn_target_t* target, sp_str_t define);
void spn_target_set_visibility(spn_target_t* target, spn_visibility_t visibility);
void spn_target_embed_file(spn_target_t* target, const c8* file);
void spn_target_embed_file_ex(spn_target_t* target, const c8* file, const c8* symbol, const c8* data_type, const c8* size_type);
void spn_target_embed_file_ex_s(spn_target_t* target, sp_str_t file, sp_str_t symbol, sp_str_t data_type, sp_str_t size_type);
void spn_target_embed_mem(spn_target_t* target, const c8* symbol, const u8* buffer, u64 buffer_size);
void spn_target_embed_mem_ex(spn_target_t* target, const c8* symbol, const u8* buffer, u64 size, const c8* data_type, const c8* size_type);
void spn_target_embed_mem_ex_s(spn_target_t* target, sp_str_t symbol, const u8* buffer, u64 size, sp_str_t data_type, sp_str_t size_type);
void spn_target_embed_dir(spn_target_t* target, const c8* dir);
void spn_target_embed_dir_ex(spn_target_t* target, const c8* dir, const c8* data_type, const c8* size_type);

#endif
