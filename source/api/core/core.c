#include "sp.h"
#include "spn.h"

#include "api/core/types.h"
#include "target/types.h"
#include "unit/types.h"

#include "intern/intern.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "target/target.h"

// Build scripts get opaque pointers; both spn_t and spn_config_t are the package unit
static spn_pkg_unit_t* to_unit(const void* spn) {
  return (spn_pkg_unit_t*)spn;
}

static spn_target_t* wrap_target(const void* spn, spn_target_info_t* info) {
  if (!info) return SP_NULLPTR;

  spn_target_t* target = sp_alloc_type(spn_target_t);
  target->spn = (spn_t*)spn;
  target->info = info;
  return target;
}

spn_target_t* spn_get_target(spn_t* spn, const c8* name) {
  return wrap_target(spn, spn_pkg_get_target(to_unit(spn)->info, name));
}

spn_target_t* spn_add_exe(spn_config_t* config, const c8* name) {
  return wrap_target(config, spn_pkg_add_exe(to_unit(config)->info, name));
}

spn_target_t* spn_add_test(spn_config_t* config, const c8* name) {
  return wrap_target(config, spn_pkg_add_test(to_unit(config)->info, name));
}

void spn_add_include(spn_config_t* config, const c8* path) {
  spn_pkg_add_include(to_unit(config)->info, path);
}

void spn_add_define(spn_config_t* config, const c8* define) {
  spn_pkg_add_define(to_unit(config)->info, define);
}

void spn_add_system_dep(spn_config_t* config, const c8* dep) {
  spn_pkg_add_system_dep(to_unit(config)->info, dep);
}

const spn_t* spn_get_dep(const spn_t* spn, const c8* name) {
  return SP_NULLPTR;
}

const c8* spn_get_subdir(const spn_t* spn, spn_dir_t base, const c8* path) {
  return "";
}

void spn_target_add_source(spn_target_t* target, const c8* source) {
  sp_da_push(target->info->source, spn_intern_cstr(source));
}

void spn_target_add_include(spn_target_t* target, const c8* include) {
  sp_da_push(target->info->include, spn_intern_cstr(include));
}

void spn_target_add_define(spn_target_t* target, const c8* define) {
  sp_da_push(target->info->define, spn_intern_cstr(define));
}

// Channel a little bit of Arthur himself to get these wrappers to fit on one line on my editor
#define view(_str) sp_str_view(_str)
#define DATA_T SP_EMBED_DEFAULT_DATA_T_S
#define SIZE_T SP_EMBED_DEFAULT_SIZE_T_S
void spn_target_embed_file(spn_target_t* t, const c8* file) {
  spn_target_embed_file_ex_s(t->info, view(file), SP_EMBED_DEFAULT_SYMBOL_S, DATA_T, SIZE_T);
}

void spn_target_embed_file_ex(spn_target_t* t, const c8* f, const c8* s, const c8* d_t, const c8* s_t) {
  spn_target_embed_file_ex_s(t->info, view(f), view(s), view(d_t), view(s_t));
}

void spn_target_embed_mem(spn_target_t* t, const c8* s, const u8* b, u64 z) {
  spn_target_embed_mem_ex_s(t->info, view(s), b, z, DATA_T, SIZE_T);
}

void spn_target_embed_mem_ex(spn_target_t* t, const c8* s, const u8* b, u64 z, const c8* d_t, const c8* s_t) {
  spn_target_embed_mem_ex_s(t->info, view(s), b, z, view(d_t), view(s_t));
}

void spn_target_embed_dir(spn_target_t* t, const c8* d) {
  spn_target_embed_dir_ex_s(t->info, view(d), DATA_T, SIZE_T);
}

void spn_target_embed_dir_ex(spn_target_t* t, const c8* d, const c8* d_t, const c8* s_t) {
  spn_target_embed_dir_ex_s(t->info, view(d), view(d_t), view(s_t));

}
