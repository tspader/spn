#include "sp.h"
#include "spn.h"

#include "api/core/types.h"
#include "target/types.h"

#include "target/target.h"
#include "intern/intern.h"

spn_target_t* spn_get_target(spn_t* spn, const c8* name) {
  return SP_NULLPTR;
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
