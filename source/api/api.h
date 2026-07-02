#ifndef SPN_API_H
#define SPN_API_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"

// Build scripts get opaque pointers; spn_t and spn_config_t are both the package unit
spn_pkg_unit_t* spn_api_unit(const void* opaque);
sp_str_t        spn_api_dir(spn_pkg_unit_t* unit, spn_dir_t dir);
sp_ps_output_t  spn_api_subprocess(sp_mem_t mem, spn_pkg_unit_t* unit, sp_ps_config_t config);

#define SPN_API_LOG(unit, fn_name, args_fmt, ...) \
  spn_event_buffer_push_ex(spn.events, (unit)->info, &(unit)->logs.io, (spn_build_event_t) { \
    .kind = SPN_EVENT_API_CALL, \
    .api_call = { .fn = sp_str_lit(fn_name), .args = sp_fmt(spn.mem, args_fmt, ##__VA_ARGS__).value } \
  })

#endif
