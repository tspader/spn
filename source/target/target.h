#ifndef SPN_TARGET_TARGET_H
#define SPN_TARGET_TARGET_H

#include "target/types.h"


void spn_target_embed_file(spn_target_t* target, const c8* file);
void spn_target_embed_file_ex(spn_target_t* target, const c8* file, const c8* symbol, const c8* data_type, const c8* size_type);
void spn_target_embed_file_ex_s(spn_target_t* target, sp_str_t file, sp_str_t symbol, sp_str_t data_type, sp_str_t size_type);
void spn_target_embed_mem(spn_target_t* target, const c8* symbol, const u8* buffer, u64 buffer_size);
void spn_target_embed_mem_ex(spn_target_t* target, const c8* symbol, const u8* buffer, u64 size, const c8* data_type, const c8* size_type);
void spn_target_embed_mem_ex_s(spn_target_t* target, sp_str_t symbol, const u8* buffer, u64 size, sp_str_t data_type, sp_str_t size_type);
void spn_target_embed_dir(spn_target_t* target, const c8* dir);
void spn_target_embed_dir_ex(spn_target_t* target, const c8* dir, const c8* data_type, const c8* size_type);

#endif
