#include "it.h"

void sp_it_dec(s64* it) {
  *it = *it - 1;
}

void sp_it_inc(s64* it) {
  *it = *it + 1;
}

bool sp_it_geq(s64 it, s64 bound) {
  return it >= bound;
}

bool sp_it_ge(s64 it, s64 bound) {
  return it > bound;
}

bool sp_it_leq(s64 it, s64 bound) {
  return it <= bound;
}

bool sp_it_le(s64 it, s64 bound) {
  return it < bound;
}

bool sp_it_eq(s64 it, s64 bound) {
  return it == bound;
}
