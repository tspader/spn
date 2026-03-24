#include "tui/spinner.h"

#include "sp/color.h"

void spn_spinner_init(spn_spinner_t* s, sp_color_t color) {
  s->interp = sp_interp_build(0.0f, 1.0f, SPN_SPINNER_CYCLE_MS);
  s->forward = true;
  s->render_forward = true;
  s->hold_ms = 0;
  s->hold_total_ms = 0;
  s->value = 0;
  s->width = 8;
  s->color = color;
}

void spn_spinner_update(spn_spinner_t* s, f32 dt_ms) {
  if (s->hold_ms > 0) {
    s->hold_ms -= dt_ms;
    if (s->hold_ms <= 0) {
      s->render_forward = s->forward;
    }
    return;
  }

  if (sp_interp_update(&s->interp, dt_ms)) {
    s->forward = !s->forward;
    s->hold_total_ms = s->forward ? SPN_SPINNER_HOLD_START_MS : SPN_SPINNER_HOLD_END_MS;
    s->hold_ms = s->hold_total_ms;
    s->interp = sp_interp_build(
      s->forward ? 0.0f : 1.0f,
      s->forward ? 1.0f : 0.0f,
      SPN_SPINNER_CYCLE_MS
    );
  }
  s->value = sp_interp_ease_inout(&s->interp);
}

sp_str_t spn_spinner_render(spn_spinner_t* s) {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  f32 active_pos = s->value * (s->width - 1);

  f32 trail_fade = 1.0f;
  if (s->hold_ms > 0 && s->hold_total_ms > 0) {
    trail_fade = s->hold_ms / s->hold_total_ms;
  }

  for (u32 i = 0; i < s->width; i++) {
    f32 pos = (f32)i;
    f32 abs_dist = active_pos - pos;
    if (abs_dist < 0) {
      abs_dist = -abs_dist;
    }

    bool is_behind = s->render_forward ? (pos < active_pos) : (pos > active_pos);

    sp_color_t color = SP_ZERO_INITIALIZE();
    const c8* glyph;

    if (abs_dist < 1.0f) {
      color = s->color;
      glyph = SPN_SPINNER_ACTIVE;
    } else if (is_behind && abs_dist < (f32)SPN_SPINNER_TRAIL_LEN) {
      f32 alpha = 1.0f;
      for (s32 j = 0; j < (s32)abs_dist; j++) {
        alpha *= 0.65f;
      }
      alpha *= trail_fade;
      f32 inactive = 0.2f;
      alpha = inactive + alpha * (1.0f - inactive);

      sp_color_t hsv = sp_color_rgb_to_hsv(s->color);
      hsv.v *= alpha;

      color = sp_color_hsv_to_rgb(hsv);
      glyph = SPN_SPINNER_ACTIVE;
    } else {
      sp_color_t hsv = sp_color_rgb_to_hsv(s->color);
      hsv.v *= 0.2f;
      color = sp_color_hsv_to_rgb(hsv);
      glyph = SPN_SPINNER_INACTIVE;
    }

    sp_str_builder_append_fmt(
      &builder,
      "{}{}",
      SP_FMT_STR(sp_color_to_tui_rgb(color)),
      SP_FMT_CSTR(glyph)
    );
  }

  sp_str_builder_append_cstr(&builder, SP_ANSI_RESET);
  return sp_str_builder_to_str(&builder);
}
