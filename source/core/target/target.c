#include "target/target.h"

#include "intern/intern.h"

void spn_target_embed_file_ex_s(spn_target_info_t* target, spn_path_t file, sp_str_t symbol, sp_str_t data_type, sp_str_t size_type) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_FILE,
    .symbol = spn_intern(symbol),
    .types = {
      .data = spn_intern(data_type),
      .size = spn_intern(size_type),
    },
    .file = {
      .path = file,
    }
  }));
}

void spn_target_embed_dir_ex_s(spn_target_info_t* target, spn_path_t dir, sp_str_t dest, sp_str_t data_type, sp_str_t size_type) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_DIR,
    .types = {
      .data = spn_intern(data_type),
      .size = spn_intern(size_type),
    },
    .dir = {
      .path = dir,
      .dest = spn_intern(dest),
    }
  }));
}
