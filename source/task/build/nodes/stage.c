#include "sp.h"
#include "unit/types.h"

#include "task/build/nodes/nodes.h"

s32 stage_targets(spn_bg_cmd_t* cmd, void* user_data) {
  spn_stage_unit_t* stage = (spn_stage_unit_t*)user_data;

  sp_fs_create_dir(stage->dir);

  sp_da_for(stage->files, it) {
    spn_stage_file_t* file = &stage->files[it];

    if (sp_fs_exists(file->to)) {
      sp_fs_remove_file(file->to);
    }
    if (sp_fs_link(file->from, file->to, SP_FS_LINK_HARD) == SP_OK) {
      continue;
    }
    if (sp_fs_link(file->from, file->to, SP_FS_LINK_COPY) != SP_OK) {
      return 1;
    }
  }

  return 0;
}
