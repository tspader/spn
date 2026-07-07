#ifndef SPN_TOML_EDIT_H
#define SPN_TOML_EDIT_H

#include "toml/types.h"
#include "spn.h"

spn_err_t              spn_toml_edit_init(spn_toml_edit_t* edit, sp_mem_t mem, sp_str_t source);
spn_toml_edit_entry_t* spn_toml_edit_find(spn_toml_edit_t* edit, const sp_str_t* path, u32 num_segments);
sp_da(sp_str_t)        spn_toml_edit_keys(spn_toml_edit_t* edit, sp_mem_t mem, const sp_str_t* path, u32 num_segments);
sp_str_t               spn_toml_edit_entry_str(spn_toml_edit_t* edit, spn_toml_edit_entry_t* entry);
spn_err_t              spn_toml_edit_set_str(spn_toml_edit_t* edit, const sp_str_t* path, u32 num_segments, sp_str_t value);
sp_str_t               spn_toml_edit_render(spn_toml_edit_t* edit, sp_mem_t mem);

#endif
