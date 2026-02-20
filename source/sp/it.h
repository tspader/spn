#ifndef SPN_SP_IT_H
#define SPN_SP_IT_H

#include "sp.h"

SP_TYPEDEF_FN(void, sp_it_next_fn_t, s64*);
SP_TYPEDEF_FN(bool, sp_it_check_fn_t, s64, s64);

typedef struct {
  s64 begin;
  s64 end;
  sp_it_next_fn_t on_next;
  sp_it_check_fn_t on_check;
  s64 n;
} sp_it_range_t;

void sp_it_dec(s64* it);
void sp_it_inc(s64* it);
bool sp_it_geq(s64 it, s64 bound);
bool sp_it_ge(s64 it, s64 bound);
bool sp_it_leq(s64 it, s64 bound);
bool sp_it_le(s64 it, s64 bound);
bool sp_it_eq(s64 it, s64 bound);
#define sp_it_for(it, begin, end, on_check, on_next) for (s64 it = begin; on_check(it, end); on_next(it))
#define sp_it_for_range(range) for (range.n = range.begin; range.on_check(range.n, range.end); range.on_next(&range.n))

#endif
