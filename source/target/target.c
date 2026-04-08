#include "target/target.h"

#include "intern/intern.h"

void spn_target_embed_file_ex_s(spn_target_info_t* target, sp_str_t file, sp_str_t symbol, sp_str_t data_type, sp_str_t size_type) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_FILE,
    .symbol = spn_intern(symbol),
    .types = {
      .data = spn_intern(data_type),
      .size = spn_intern(size_type),
    },
    .file = {
      .path = spn_intern(file),
    }
  }));
}

void spn_target_embed_mem_ex_s(spn_target_info_t* target, sp_str_t symbol, const u8* buffer, u64 size, sp_str_t data_type, sp_str_t size_type) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_MEM,
    .symbol = spn_intern(symbol),
    .types = {
      .data = spn_intern(data_type),
      .size = spn_intern(size_type),
    },
    .memory = {
      .buffer = buffer,
      .size = size
    }
  }));
}

void spn_target_embed_dir_ex_s(spn_target_info_t* target, sp_str_t dir, sp_str_t data_type, sp_str_t size_type) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_DIR,
    .types = {
      .data = spn_intern(data_type),
      .size = spn_intern(size_type),
    },
    .dir = {
      .path = spn_intern(dir),
    }
  }));

}

