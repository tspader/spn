#ifndef SPN_SPN_H
#define SPN_SPN_H

#ifdef SPN_HOST_H
  #error "include/spn.h is the guest build script API; it cannot share a translation unit with spn/host.h"
#endif

#include "spn/core.h"

#ifdef __wasm32__
  #define SPN_API __attribute__((import_module("env")))
  #define SPN_EXPORT __attribute__((used, visibility("default")))
#else
  #define SPN_API
  #define SPN_EXPORT
#endif

typedef struct spn              spn_t;
typedef struct spn_config       spn_config_t;
typedef struct spn_target       spn_target_t;
typedef struct spn_profile      spn_profile_t;
typedef struct spn_node         spn_node_t;

typedef spn_err_t (*spn_configure_fn_t) (spn_t*, spn_config_t*);
typedef s32       (*spn_node_fn_t)      (spn_t*);

spn_target_t* spn_get_target(spn_t* spn, const c8* name);
const spn_t*  spn_get_dep(const spn_t* spn, const c8* name);
const c8*     spn_get_dir(const spn_t* spn, spn_dir_t dir);
const c8*     spn_get_subdir(const spn_t* spn, spn_dir_t base, const c8* path);
void          spn_write_file(spn_t* spn, const c8* path, const c8* content);
s32           spn_copy(spn_t* spn, spn_dir_t from, const c8* from_path, spn_dir_t to, const c8* to_path);
void          spn_log(spn_t* spn, const c8* message);

void          spn_fs_copy(const c8* from, const c8* to);
void          spn_fs_copy_glob(const c8* glob, const c8* dir);
void          spn_fs_create_dir(const c8* path);
void          spn_io_write(const c8* path, const c8* contents);
const c8*     spn_fmt_ex(const c8* fmt, const c8* a0, const c8* a1, const c8* a2, const c8* a3);

#define spn_fmt_select(fmt, a0, a1, a2, a3, ...) spn_fmt_ex(fmt, a0, a1, a2, a3)
#define spn_fmt(fmt, ...) spn_fmt_select(fmt, ##__VA_ARGS__, 0, 0, 0, 0, 0)

spn_target_t* spn_add_exe(spn_config_t* config, const c8* name);
spn_target_t* spn_add_lib(spn_config_t* config, const c8* name, spn_linkage_t kind);
spn_target_t* spn_add_test(spn_config_t* config, const c8* name);
void          spn_add_include(spn_config_t* config, const c8* path);
void          spn_add_define(spn_config_t* config, const c8* define);
void          spn_add_system_dep(spn_config_t* config, const c8* dep);
spn_node_t*   spn_add_node(spn_config_t* config, const c8* tag);
void          spn_node_add_input(spn_node_t* node, const c8* input);
void          spn_node_add_output(spn_node_t* node, const c8* output);
void          spn_node_link(spn_node_t* from, spn_node_t* to);
void          spn_node_set_fn(spn_node_t* node, const c8* fn);

void          spn_target_add_source(spn_target_t* target, const c8* source);
void          spn_target_add_include(spn_target_t* target, const c8* include);
void          spn_target_add_define(spn_target_t* target, const c8* define);
void          spn_target_add_flag(spn_target_t* target, const c8* flag);
void          spn_target_embed_file(spn_target_t* target, const c8* file);
void          spn_target_embed_file_ex(spn_target_t* target, const c8* file, const c8* symbol, const c8* data_type, const c8* size_type);
void          spn_target_embed_dir_ex(spn_target_t* target, const c8* dir, const c8* dest, const c8* data_type, const c8* size_type);

spn_profile_t*   spn_get_profile(spn_t* spn);
spn_libc_kind_t  spn_profile_get_libc(spn_profile_t* profile);
spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile);
spn_opt_level_t  spn_profile_get_opt(spn_profile_t* profile);
spn_sanitizer_set_t spn_profile_get_sanitizers(spn_profile_t* profile);

#endif
