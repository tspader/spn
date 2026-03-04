#include "tm.h"

#include <time.h>

bool sp_tm_epoch_gt(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s > b.s || (a.s == b.s && a.ns > b.ns);
}

bool sp_tm_epoch_ge(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s > b.s || (a.s == b.s && a.ns >= b.ns);
}

bool sp_tm_epoch_lt(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s < b.s || (a.s == b.s && a.ns < b.ns);
}

bool sp_tm_epoch_le(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s < b.s || (a.s == b.s && a.ns <= b.ns);
}

bool sp_tm_epoch_eq(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s == b.s && a.ns == b.ns;
}

sp_tm_epoch_t sp_tm_epoch_min(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return sp_tm_epoch_lt(a, b) ? a : b;
}

sp_tm_epoch_t sp_tm_epoch_max(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return sp_tm_epoch_gt(a, b) ? a : b;
}

sp_str_t sp_tm_epoch_to_iso8601_us(sp_tm_epoch_t time) {
  struct tm* time_info;
  time_t raw_time = (time_t)time.s;
  time_info = gmtime(&raw_time);

  c8 buffer[32];
  size_t len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", time_info);

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&builder, sp_str(buffer, len));
  sp_str_builder_append_c8(&builder, '.');

  u32 us = time.ns / 1000;
  if (us < 100000) sp_str_builder_append_c8(&builder, '0');
  if (us < 10000)  sp_str_builder_append_c8(&builder, '0');
  if (us < 1000)   sp_str_builder_append_c8(&builder, '0');
  if (us < 100)    sp_str_builder_append_c8(&builder, '0');
  if (us < 10)     sp_str_builder_append_c8(&builder, '0');
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_U32(us));
  sp_str_builder_append_c8(&builder, 'Z');

  return sp_str_builder_to_str(&builder);
}
