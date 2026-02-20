#include "tmp.c"
#include "git.c"

s32 dir_entry_sort_kernel_by_name(const void* a, const void* b) {
  const sp_os_dir_ent_t* lhs = (const sp_os_dir_ent_t*)a;
  const sp_os_dir_ent_t* rhs = (const sp_os_dir_ent_t*)b;
  return sp_str_sort_kernel_alphabetical(&lhs->file_name, &rhs->file_name);
}
