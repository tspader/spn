#include "tm.h"

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
