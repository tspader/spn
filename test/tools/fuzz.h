#ifndef SPN_TEST_FUZZ_H
#define SPN_TEST_FUZZ_H

#include "sp.h"

#include "intern/types.h"

typedef struct {
  u64 state;
} prng_t;

u64  prng_next(prng_t* prng);
u64  prng_below(prng_t* prng, u64 bound);
bool prng_chance(prng_t* prng, u32 numerator, u32 denominator);

u64          fuzz_seed_init(void);
prng_t       fuzz_stream(sp_da(sp_str_t) names);
sp_intern_t* fuzz_perturbed_intern(prng_t* prng, sp_da(sp_str_t) names);

#endif
