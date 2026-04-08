#include "target/target.h"

#include "ctx/ctx.h"
#include "event/event.h"
#include "intern.h"

void spn_target_embed_file(spn_target_info_t* target, const c8* file) {
  spn_target_embed_file_ex_s(target, sp_str_view(file), SP_EMBED_DEFAULT_SYMBOL_S, SP_EMBED_DEFAULT_DATA_T_S, SP_EMBED_DEFAULT_SIZE_T_S);
}

void spn_target_embed_file_ex(spn_target_info_t* target, const c8* file, const c8* symbol, const c8* data_type, const c8* size_type) {
  spn_target_embed_file_ex_s(target, sp_str_view(file), sp_str_view(symbol), sp_str_view(data_type), sp_str_view(size_type));
}

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

void spn_target_embed_mem(spn_target_info_t* target, const c8* symbol, const u8* buffer, u64 buffer_size) {
  spn_target_embed_mem_ex(target, symbol, buffer, buffer_size, SP_EMBED_DEFAULT_DATA_T, SP_EMBED_DEFAULT_SIZE_T);
}

void spn_target_embed_mem_ex(spn_target_info_t* target, const c8* symbol, const u8* buffer, u64 size, const c8* data_type, const c8* size_type) {
  spn_target_embed_mem_ex_s(target, sp_str_view(symbol), buffer, size, sp_str_view(data_type), sp_str_view(size_type));
}

void spn_target_embed_dir(spn_target_info_t* target, const c8* dir) {
  spn_target_embed_dir_ex(target, dir, SP_EMBED_DEFAULT_DATA_T, SP_EMBED_DEFAULT_SIZE_T);
}

void spn_target_embed_dir_ex(spn_target_info_t* target, const c8* dir, const c8* data_type, const c8* size_type) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_DIR,
    .types = {
      .data = spn_intern_cstr(data_type),
      .size = spn_intern_cstr(size_type),
    },
    .dir = {
      .path = spn_intern_cstr(dir),
    }
  }));
}
