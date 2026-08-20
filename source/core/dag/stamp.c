#include "dag/stamp.h"

#include "sp/fs.h"

bool is_timestamp_fenced(sp_sys_timespec_t fence, sp_sys_timespec_t mtime) {
  if (mtime.tv_sec != fence.tv_sec) {
    return mtime.tv_sec < fence.tv_sec;
  }
  return mtime.tv_nsec < fence.tv_nsec;
}

spn_err_t cache_timestamp_fence(sp_str_t dir, sp_sys_timespec_t* fence) {
  *fence = (sp_sys_timespec_t) sp_zero;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_str_t path = sp_fs_join_path(s.mem, dir, sp_str_lit(".fence"));
  spn_err_t err = SPN_OK;
  if (sp_fs_create_file_str(path, sp_str_lit("fence"))) {
    err = SPN_ERR_DAG_SCRATCH;
  }
  else {
    sp_sys_file_meta_t meta = sp_zero;
    if (sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &meta)) {
      err = SPN_ERR_DAG_STAT;
    }
    else {
      *fence = meta.mtime;
    }
  }
  sp_mem_end_scratch(s);
  return err;
}
