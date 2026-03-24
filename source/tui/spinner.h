#ifndef SPN_TUI_SPINNER_H
#define SPN_TUI_SPINNER_H

#include "tui/types.h"

void spn_spinner_init(spn_spinner_t* s, sp_color_t color);
void spn_spinner_update(spn_spinner_t* s, f32 dt_ms);
sp_str_t spn_spinner_render(spn_spinner_t* s);

#endif
