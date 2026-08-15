#ifndef SPN_TARGET_TARGET_H
#define SPN_TARGET_TARGET_H

#include "paths/types.h"
#include "target/types.h"

void spn_target_embed_file_ex_s(spn_target_info_t* target, spn_path_t file, sp_str_t symbol, sp_str_t data_type, sp_str_t size_type);
void spn_target_embed_dir_ex_s(spn_target_info_t* target, spn_path_t dir, sp_str_t dest, sp_str_t data_type, sp_str_t size_type);

#endif
