#define SP_PROMPT_IMPLEMENTATION
#include "tail.h"

#define TAIL_ROWS 8
#define TAIL_COLS 256

typedef struct {
  const c8* title;
  sp_ps_t ps;
  sp_io_reader_t* reader;
  s32 status;
  u32 frame;
  struct {
    c8 data [TAIL_ROWS][TAIL_COLS];
    u32 lens [TAIL_ROWS];
    u64 total;
  } lines;
  struct {
    c8 data [TAIL_COLS];
    u32 len;
  } partial;
} tail_t;

static const u32 frames [] = SP_PROMPT_SPINNER_BRAILLE_CIRCLING_DOT;

static sp_prompt_style_t ansi(u8 code) {
  return (sp_prompt_style_t) { .tag = SP_PROMPT_STYLE_ANSI, .ansi = code };
}

static void push(tail_t* tail, c8 c) {
  if (tail->partial.len < TAIL_COLS) {
    tail->partial.data[tail->partial.len++] = c;
  }
}

static void commit(tail_t* tail) {
  u32 slot = sp_cast(u32, tail->lines.total % TAIL_ROWS);
  sp_mem_copy(tail->lines.data[slot], tail->partial.data, tail->partial.len);
  tail->lines.lens[slot] = tail->partial.len;
  tail->lines.total++;
  tail->partial.len = 0;
}

static void ingest(tail_t* tail, sp_str_t chunk) {
  sp_str_for(chunk, it) {
    c8 c = chunk.data[it];
    switch (c) {
      case '\n': { commit(tail); break; }
      case '\r': { break; }
      case '\t': { push(tail, ' '); break; }
      default:   { push(tail, c); break; }
    }
  }
}

static void drain(tail_t* tail) {
  while (true) {
    c8 buffer [4096];
    u64 bytes = 0;
    if (sp_io_read(tail->reader, buffer, sizeof(buffer), &bytes)) {
      return;
    }
    if (!bytes) {
      return;
    }
    ingest(tail, sp_str(buffer, sp_cast(u32, bytes)));
  }
}

static void on_event(sp_prompt_ctx_t* ctx, sp_prompt_event_t event) {
  switch (event.kind) {
    case SP_PROMPT_EVENT_CTRL_C:
    case SP_PROMPT_EVENT_ESCAPE: {
      sp_prompt_set_state(ctx, SP_PROMPT_STATE_CANCEL);
      break;
    }
    default: {
      break;
    }
  }
}

static void on_update(sp_prompt_ctx_t* ctx) {
  tail_t* tail = sp_prompt_get_user_data(ctx);
  tail->frame = (tail->frame + 1) % sp_carr_len(frames);

  drain(tail);

  sp_ps_status_t status = sp_ps_poll(&tail->ps, 0);
  if (status.state != SP_PS_STATE_DONE) {
    return;
  }

  drain(tail);
  if (tail->partial.len) {
    commit(tail);
  }
  tail->status = status.exit_code;
  sp_prompt_set_state(ctx, status.exit_code ? SP_PROMPT_STATE_ERROR : SP_PROMPT_STATE_SUBMIT);
}

static void rail(sp_prompt_ctx_t* ctx, sp_prompt_style_t style) {
  sp_prompt_render_line(ctx, sp_str_lit("│"), style);
  sp_prompt_render_line(ctx, sp_str_lit("  "), sp_zero_s(sp_prompt_style_t));
}

static void border(sp_prompt_ctx_t* ctx, const c8* left, const c8* right, u32 interior, sp_prompt_style_t rail_style, sp_prompt_style_t border_style) {
  rail(ctx, rail_style);
  sp_prompt_render_line(ctx, sp_cstr_as_str(left), border_style);
  sp_prompt_render_line(ctx, sp_prompt_repeat(ctx, 0x2500, interior + 2), border_style);
  sp_prompt_render_line(ctx, sp_cstr_as_str(right), border_style);
  sp_prompt_line(ctx, sp_str_lit(""));
}

static void row(sp_prompt_ctx_t* ctx, sp_str_t text, u32 interior, sp_prompt_style_t rail_style, sp_prompt_style_t border_style, sp_prompt_style_t style) {
  rail(ctx, rail_style);
  sp_prompt_render_line(ctx, sp_str_lit("│ "), border_style);

  u32 width = 0;
  u32 bytes = 0;
  sp_str_for_utf8(text, it) {
    if (width == interior) {
      break;
    }
    width++;
    bytes = sp_cast(u32, it.index) + it.codepoint_len;
  }
  sp_prompt_render_line(ctx, sp_str(text.data, bytes), style);
  sp_prompt_render_line(ctx, sp_prompt_repeat(ctx, ' ', interior - width), sp_zero_s(sp_prompt_style_t));
  sp_prompt_render_line(ctx, sp_str_lit(" │"), border_style);
  sp_prompt_line(ctx, sp_str_lit(""));
}

static void render(sp_prompt_ctx_t* ctx) {
  tail_t* tail = sp_prompt_get_user_data(ctx);
  sp_prompt_state_t state = sp_cast(sp_prompt_state_t, ctx->state);

  sp_str_t symbol = sp_zero;
  sp_prompt_style_t symbol_style = sp_zero;
  switch (state) {
    case SP_PROMPT_STATE_ACTIVE: {
      symbol = sp_prompt_repeat(ctx, frames[tail->frame], 1);
      symbol_style = ansi(SP_ANSI_FG_BLUE_U8);
      break;
    }
    case SP_PROMPT_STATE_SUBMIT: {
      symbol = sp_str_lit("◇");
      break;
    }
    case SP_PROMPT_STATE_CANCEL: {
      symbol = sp_str_lit("■");
      symbol_style = ansi(SP_ANSI_FG_RED_U8);
      break;
    }
    case SP_PROMPT_STATE_ERROR: {
      symbol = sp_str_lit("▲");
      symbol_style = ansi(SP_ANSI_FG_RED_U8);
      break;
    }
  }

  sp_prompt_render_line(ctx, symbol, symbol_style);
  sp_prompt_render_line(ctx, sp_str_lit("  "), sp_zero_s(sp_prompt_style_t));
  sp_prompt_render_line(ctx, sp_cstr_as_str(tail->title), sp_zero_s(sp_prompt_style_t));
  sp_prompt_line(ctx, sp_str_lit(""));

  if (state == SP_PROMPT_STATE_SUBMIT) {
    return;
  }

  sp_prompt_style_t rail_style = state == SP_PROMPT_STATE_ACTIVE ? ansi(SP_ANSI_FG_BLUE_U8) : sp_zero_s(sp_prompt_style_t);
  sp_prompt_style_t border_style = state == SP_PROMPT_STATE_ERROR ? ansi(SP_ANSI_FG_RED_U8) : ansi(SP_ANSI_FG_BRIGHT_BLACK_U8);
  sp_prompt_style_t text = state == SP_PROMPT_STATE_ACTIVE ? ansi(SP_ANSI_FG_BRIGHT_BLACK_U8) : sp_zero_s(sp_prompt_style_t);
  u32 interior = ctx->cols >= 16 ? ctx->cols - 8 : 8;

  border(ctx, "╭", "╮", interior, rail_style, border_style);
  u64 count = sp_min(tail->lines.total, TAIL_ROWS);
  sp_for(it, count) {
    u64 index = tail->lines.total - count + it;
    u32 slot = sp_cast(u32, index % TAIL_ROWS);
    sp_str_t line = sp_str(tail->lines.data[slot], tail->lines.lens[slot]);
    if (!it && tail->lines.total > TAIL_ROWS) {
      line = sp_str_lit("…");
    }
    row(ctx, line, interior, rail_style, border_style, text);
  }
  border(ctx, "╰", "╯", interior, rail_style, border_style);

  sp_prompt_render_line(ctx, sp_str_lit("└"), rail_style);
  sp_prompt_line(ctx, sp_str_lit(""));
}

s32 tail_trace(sp_mem_t mem, sp_prompt_ctx_t* prompt, const c8* title, sp_ps_config_t config) {
  config.io.out = (sp_ps_io_out_config_t) { .mode = SP_PS_IO_MODE_CREATE };
  config.io.err = (sp_ps_io_out_config_t) { .mode = SP_PS_IO_MODE_REDIRECT };

  tail_t tail = sp_zero;
  tail.title = title;
  tail.status = -1;
  tail.ps = sp_ps_create(mem, config);
  if (!tail.ps.os) {
    return -1;
  }
  tail.reader = sp_ps_io_out(&tail.ps);

  sp_prompt_run(prompt, (sp_prompt_widget_t) {
    .user_data = &tail,
    .on_event = on_event,
    .on_update = on_update,
    .render = render,
  });

  if (sp_prompt_cancelled(prompt)) {
    sp_ps_kill(&tail.ps);
    sp_ps_wait(&tail.ps);
  }
  sp_ps_free(&tail.ps);
  return tail.status;
}
