#ifndef SPN_TARGET_TARGET_H
#define SPN_TARGET_TARGET_H

#include "target/types.h"

void spn_target_embed_file_ex_s(spn_target_info_t* target, sp_str_t file, sp_str_t symbol, sp_str_t data_type, sp_str_t size_type);
void spn_target_embed_mem_ex_s(spn_target_info_t* target, sp_str_t symbol, const u8* buffer, u64 size, sp_str_t data_type, sp_str_t size_type);
void spn_target_embed_dir_ex_s(spn_target_info_t* target, sp_str_t dir, sp_str_t data_type, sp_str_t size_type);

#endif
