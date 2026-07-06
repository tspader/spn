#ifndef SP_FUZZ_H
#define SP_FUZZ_H

#include "sp.h"

#include "intern/types.h"

typedef struct {
  u64 state;
} sp_fuzz_prng_t;

typedef struct {
  u64 iters;
  s64 only;
} sp_fuzz_opts_t;

u64  sp_fuzz_next(sp_fuzz_prng_t* prng);
u64  sp_fuzz_below(sp_fuzz_prng_t* prng, u64 bound);
u64  sp_fuzz_range(sp_fuzz_prng_t* prng, u64 min, u64 max);
bool sp_fuzz_chance(sp_fuzz_prng_t* prng, u32 numerator, u32 denominator);
u32  sp_fuzz_weighted(sp_fuzz_prng_t* prng, const u64* weights, u32 count);
void sp_fuzz_shuffle(sp_fuzz_prng_t* prng, void* base, u64 count, u64 stride);
void sp_fuzz_swarm(sp_fuzz_prng_t* prng, u64* weights, u32 count);

u64            sp_fuzz_seed_init(void);
sp_fuzz_prng_t sp_fuzz_stream(sp_da(sp_str_t) names);
sp_fuzz_prng_t sp_fuzz_iter(sp_fuzz_prng_t base, u64 iter);
sp_fuzz_opts_t sp_fuzz_opts(u64 default_iters);

sp_intern_t* sp_fuzz_perturbed_intern(sp_fuzz_prng_t* prng, sp_da(sp_str_t) names);

#endif
