#ifndef SPN_API_H
#define SPN_API_H

#include "sp.h"
#include "spn/core.h"

#include "core/types.h"

// Build scripts get opaque pointers; spn_t and spn_config_t are both the package unit
spn_pkg_unit_t* spn_api_unit(const void* opaque);
sp_str_t        spn_api_dir(spn_pkg_unit_t* unit, spn_dir_t dir);
s32             spn_api_copy(sp_str_t from, sp_str_t to);
s32             spn_api_copy_rooted(spn_pkg_unit_t* unit, spn_dir_t from_dir, sp_str_t from_path, spn_dir_t to_dir, sp_str_t to_path);

#define SPN_API_LOG(unit, fn_name, args_fmt, ...) \
  spn_event_buffer_push_ex(spn.events, (unit)->info, &(unit)->logs.io, (spn_build_event_t) { \
    .kind = SPN_EVENT_API_CALL, \
    .api_call = { .fn = sp_str_lit(fn_name), .args = sp_fmt(spn.mem, args_fmt, ##__VA_ARGS__).value } \
  })

#endif
