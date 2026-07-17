#include "dag/occ.h"

#define occ_try(expr) do { occ_err_t __err = (expr); if (__err) return __err; } while (0)

static bool occ_is_sep(c8 c) {
  return (c == '/') || (c == '\\');
}

static bool occ_is_eol(c8 c) {
  return (c == '\r') || (c == '\n');
}

static bool occ_is_hspace(c8 c) {
  return (c == ' ') || (c == '\t');
}

static bool occ_is_whitespace(c8 c) {
  return occ_is_hspace(c) || occ_is_eol(c);
}

static bool occ_is_valid(occ_parser_t* p, u64 index) {
  return index < p->content.len;
}

static bool occ_is_done(occ_parser_t* p) {
  return !occ_is_valid(p, p->n);
}

static c8 occ_peek(occ_parser_t* p) {
  if (occ_is_done(p)) return 0;
  return p->content.data[p->n];
}

static c8 occ_look(occ_parser_t* p, u64 ahead) {
  u64 index = p->n + ahead;
  if (!occ_is_valid(p, index)) return 0;
  return p->content.data[index];
}

static c8 occ_eat(occ_parser_t* p) {
  if (occ_is_done(p)) return 0;
  return p->content.data[p->n++];
}

static bool occ_match(occ_parser_t* p, c8 c) {
  return occ_peek(p) == c;
}

static bool occ_match_eat(occ_parser_t* p, c8 c) {
  if (!occ_match(p, c)) return false;
  occ_eat(p);
  return true;
}

static bool occ_match_sep(occ_parser_t* p) {
  return occ_is_sep(occ_peek(p));
}

static bool occ_match_eol(occ_parser_t* p) {
  return occ_is_done(p) || occ_is_eol(occ_peek(p));
}

static bool occ_match_hspace(occ_parser_t* p) {
  return occ_is_hspace(occ_peek(p));
}

static bool occ_match_whitespace(occ_parser_t* p) {
  return occ_is_done(p) || occ_is_whitespace(occ_peek(p));
}

static c8 occ_eat_hspace(occ_parser_t* p) {
  while (occ_match_hspace(p)) {
    occ_eat(p);
  }
  return occ_peek(p);
}

static c8 occ_eat_whitespace(occ_parser_t* p) {
  while (!occ_is_done(p) && occ_match_whitespace(p)) {
    occ_eat(p);
  }
  return occ_peek(p);
}

static occ_err_t occ_parse_target(occ_parser_t* p) {
  while (!occ_match_eol(p)) {
    if (occ_match_eat(p, ':')) {
      if (occ_match_sep(p)) continue;
      return OCC_OK;
    }
    occ_eat(p);
  }
  return OCC_ERR_MISSING_COLON;
}

static bool occ_seek(occ_parser_t* p) {
  if (p->err) return false;

  while (true) {
    if (!occ_eat_hspace(p)) return false;

    if (occ_match_eol(p)) {
      if (!occ_eat_whitespace(p)) return false;
      p->err = occ_parse_target(p);
      if (p->err) return false;
      continue;
    }

    if (!occ_match(p, '\\')) return true;

    c8 next = occ_look(p, 1);
    if (!next) {
      occ_eat(p);
      return false;
    }
    if (next == '\n') {
      p->n += 2;
      continue;
    }
    if (next == '\r' && occ_look(p, 2) == '\n') {
      p->n += 3;
      continue;
    }
    if (next == '\r') {
      p->err = OCC_ERR_BAD_CONTINUATION;
      return false;
    }
    return true;
  }
}

static occ_err_t occ_push(occ_parser_t* p, u64* len, c8 c) {
  if (*len >= SP_PATH_MAX) return OCC_ERR_PREREQ_TOO_LONG;
  p->buf[(*len)++] = c;
  return OCC_OK;
}

static occ_err_t occ_parse_one(occ_parser_t* p, sp_str_t* prereq) {
  u64 len = 0;

  if (occ_match_eat(p, '"')) {
    while (!occ_match_eol(p) && !occ_match(p, '"')) {
      occ_try(occ_push(p, &len, occ_eat(p)));
    }

    if (!occ_match_eat(p, '"')) return OCC_ERR_UNTERMINATED_QUOTE;
  }
  else {
    while (!occ_match_whitespace(p)) {
      if (occ_match(p, '\\')) {
        u64 slashes = 0;
        while (occ_look(p, slashes) == '\\') slashes++;
        c8 next = occ_look(p, slashes);

        if (!next) {
          p->n += slashes;
          sp_for(it, slashes - 1) {
            occ_try(occ_push(p, &len, '\\'));
          }
          break;
        }
        if (occ_is_eol(next)) {
          p->n += slashes - 1;
          sp_for(it, slashes - 1) {
            occ_try(occ_push(p, &len, '\\'));
          }
          break;
        }
        if (next == ' ' || next == '\t') {
          p->n += slashes + 1;
          sp_for(it, slashes / 2) {
            occ_try(occ_push(p, &len, '\\'));
          }
          occ_try(occ_push(p, &len, next));
          continue;
        }
        if (next == '#') {
          p->n += slashes + 1;
          sp_for(it, slashes - 1) {
            occ_try(occ_push(p, &len, '\\'));
          }
          occ_try(occ_push(p, &len, '#'));
          continue;
        }
      }

      if (occ_match(p, '$') && occ_look(p, 1) == '$') {
        p->n += 2;
        occ_try(occ_push(p, &len, '$'));
        continue;
      }

      occ_try(occ_push(p, &len, occ_eat(p)));
    }
  }

  *prereq = (sp_str_t) {
    .data = p->buf,
    .len = len,
  };
  return OCC_OK;
}

occ_err_t occ_init(occ_parser_t* p, sp_str_t content) {
  *p = sp_zero_s(occ_parser_t);
  p->content = content;

  if (!occ_eat_whitespace(p)) return OCC_OK;

  p->err = occ_parse_target(p);
  return p->err;
}

bool occ_next(occ_parser_t* p, sp_str_t* prereq) {
  if (!occ_seek(p)) return false;

  p->err = occ_parse_one(p, prereq);
  return !p->err;
}
