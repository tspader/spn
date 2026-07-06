#include "fuzz.h"

#include "intern/intern.h"

static u64 fuzz_seed;

u64 prng_next(prng_t* prng) {
  prng->state += 0x9e3779b97f4a7c15u;
  u64 mixed = prng->state;
  mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9u;
  mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebu;
  return mixed ^ (mixed >> 31);
}

u64 prng_below(prng_t* prng, u64 bound) {
  return bound ? prng_next(prng) % bound : 0;
}

bool prng_chance(prng_t* prng, u32 numerator, u32 denominator) {
  return prng_below(prng, denominator) < numerator;
}

u64 fuzz_seed_init(void) {
  const c8* seed_env = getenv("SPN_TEST_SEED");
  if (seed_env) {
    fuzz_seed = strtoull(seed_env, SP_NULLPTR, 0);
  }
  else {
    prng_t entropy = { .state = (u64)time(SP_NULLPTR) ^ ((u64)clock() << 32) };
    fuzz_seed = prng_next(&entropy);
  }
  printf("SPN_TEST_SEED=0x%016llx\n", (unsigned long long)fuzz_seed);
  return fuzz_seed;
}

// Derive the stream from the seed and the name set, not from a shared stream
// position, so a failure reproduces under --filter
prng_t fuzz_stream(sp_da(sp_str_t) names) {
  prng_t prng = { .state = fuzz_seed };
  sp_da_for(names, it) {
    prng.state = sp_hash_bytes(names[it].data, names[it].len, prng.state);
  }
  return prng;
}

// Pre-intern a random subset of the names in a random order, with junk ids
// burned in between, so real names draw arbitrary ids instead of first-touch
// order. '#' can't appear in a qualified name, so junk never collides.
sp_intern_t* fuzz_perturbed_intern(prng_t* prng, sp_da(sp_str_t) names) {
  sp_intern_t* intern = sp_intern_new(sp_mem_os_new());

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_str_t) shuffled = sp_da_new(scratch.mem, sp_str_t);
  sp_da_for(names, it) {
    sp_da_push(shuffled, names[it]);
  }
  for (u64 it = sp_da_size(shuffled); it > 1; it--) {
    u64 jt = prng_below(prng, it);
    sp_str_t held = shuffled[it - 1];
    shuffled[it - 1] = shuffled[jt];
    shuffled[jt] = held;
  }

  u32 junk = 0;
  sp_da_for(shuffled, it) {
    while (prng_chance(prng, 1, 2)) {
      c8 buffer[16];
      snprintf(buffer, sizeof(buffer), "#%u", junk++);
      sp_intern_get_or_insert(intern, sp_str_view(buffer));
    }
    if (prng_chance(prng, 1, 2)) {
      sp_intern_get_or_insert(intern, shuffled[it]);
    }
  }

  sp_mem_end_scratch(scratch);
  return intern;
}
