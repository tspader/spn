#include "sp.h"
#include "git/patch.h"

spn_err_t spn_git_patch_set_load(sp_mem_t mem, sp_da(sp_str_t) files, spn_git_patch_set_t* out, sp_str_t* missing) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_hash_t hash = 0;
  sp_da_for(files, it) {
    sp_str_t content = sp_zero;
    if (sp_io_read_file(scratch.mem, files[it], &content)) {
      *missing = files[it];
      sp_mem_end_scratch(scratch);
      return SPN_ERROR;
    }
    sp_hash_t parts [] = {
      hash,
      sp_hash_bytes(content.data, content.len, 0),
    };
    hash = sp_hash_combine(parts, sp_carr_len(parts));
  }

  sp_mem_end_scratch(scratch);

  out->files = sp_da_new(mem, sp_str_t);
  sp_da_for(files, it) {
    sp_da_push(out->files, sp_str_copy(mem, files[it]));
  }
  out->hash = hash;
  return SPN_OK;
}
