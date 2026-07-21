#include "sp.h"
#include "git/patch.h"

spn_err_t spn_git_patch_set_hash(spn_git_patch_set_t* set, u32* missing) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  set->hash = 0;
  sp_da_for(set->files, it) {
    sp_str_t content = sp_zero;
    if (sp_io_read_file(scratch.mem, set->files[it], &content)) {
      *missing = (u32)it;
      set->hash = 0;
      sp_mem_end_scratch(scratch);
      return SPN_ERROR;
    }
    sp_hash_t parts [] = {
      set->hash,
      sp_hash_bytes(content.data, content.len, 0),
    };
    set->hash = sp_hash_combine(parts, sp_carr_len(parts));
  }

  sp_mem_end_scratch(scratch);
  sp_assert(sp_da_empty(set->files) || set->hash);
  return SPN_OK;
}
