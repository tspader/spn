#ifndef SPN_SPINNER_H
#define SPN_SPINNER_H

#include "sp.h"

#define SPN_SPINNER_TRAIL_LEN 6
#define SPN_SPINNER_CYCLE_MS 500.0f
#define SPN_SPINNER_HOLD_START_MS 500.0f
#define SPN_SPINNER_HOLD_END_MS 100.0f

#define SPN_SPINNER_ACTIVE "\u25A0"
#define SPN_SPINNER_INACTIVE "\u2B1D"

typedef struct spn_spinner_t {
  sp_interp_t interp;
  bool forward;
  bool render_forward;
  f32 hold_ms;
  f32 hold_total_ms;
  f32 value;
  u32 width;
  sp_color_t color;
} spn_spinner_t;

void spn_spinner_init(spn_spinner_t* s, sp_color_t color);
void spn_spinner_update(spn_spinner_t* s, f32 dt_ms);
sp_str_t spn_spinner_render(spn_spinner_t* s);

#endif
