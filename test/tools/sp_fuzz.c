#include "sp_fuzz.h"

#include "intern/intern.h"

static u64 sp_fuzz_seed;

u64 sp_fuzz_next(sp_fuzz_prng_t* prng) {
  prng->state += 0x9e3779b97f4a7c15u;
  u64 mixed = prng->state;
  mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9u;
  mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebu;
  return mixed ^ (mixed >> 31);
}

u64 sp_fuzz_below(sp_fuzz_prng_t* prng, u64 bound) {
  return bound ? sp_fuzz_next(prng) % bound : 0;
}

u64 sp_fuzz_range(sp_fuzz_prng_t* prng, u64 min, u64 max) {
  sp_assert(min <= max);
  return min + sp_fuzz_below(prng, max - min + 1);
}

bool sp_fuzz_chance(sp_fuzz_prng_t* prng, u32 numerator, u32 denominator) {
  return sp_fuzz_below(prng, denominator) < numerator;
}

u32 sp_fuzz_weighted(sp_fuzz_prng_t* prng, const u64* weights, u32 count) {
  u64 total = 0;
  for (u32 it = 0; it < count; it++) {
    total += weights[it];
  }
  if (!total) {
    return (u32)sp_fuzz_below(prng, count);
  }

  u64 pick = sp_fuzz_below(prng, total);
  for (u32 it = 0; it < count; it++) {
    if (pick < weights[it]) {
      return it;
    }
    pick -= weights[it];
  }
  sp_unreachable_return(0);
}

void sp_fuzz_shuffle(sp_fuzz_prng_t* prng, void* base, u64 count, u64 stride) {
  u8* bytes = (u8*)base;
  for (u64 it = count; it > 1; it--) {
    u64 jt = sp_fuzz_below(prng, it);
    u8* a = bytes + (it - 1) * stride;
    u8* b = bytes + jt * stride;
    for (u64 kt = 0; kt < stride; kt++) {
      u8 held = a[kt];
      a[kt] = b[kt];
      b[kt] = held;
    }
  }
}

// Swarm testing: a random subset of variants is enabled at all, and the
// enabled ones get wildly different weights, so each run explores a
// qualitatively different regime instead of an average over all of them
void sp_fuzz_swarm(sp_fuzz_prng_t* prng, u64* weights, u32 count) {
  u32 enabled = (u32)sp_fuzz_range(prng, 1, count);
  u32 remaining = count;
  for (u32 it = 0; it < count; it++) {
    if (sp_fuzz_below(prng, remaining) < enabled) {
      weights[it] = sp_fuzz_range(prng, 1, 100);
      enabled--;
    }
    else {
      weights[it] = 0;
    }
    remaining--;
  }
}

static u64 sp_fuzz_parse_u64(sp_str_t str) {
  if (str.len > 2 && str.data[0] == '0' && (str.data[1] == 'x' || str.data[1] == 'X')) {
    return sp_parse_hex(str);
  }
  return sp_parse_u64(str);
}

static sp_err_t sp_fuzz_fmt_hex(sp_io_writer_t* io, sp_fmt_arg_t* arg) {
  return sp_fmt_write_u64_ex(io, arg->value.u, SP_FMT_RADIX_HEX);
}

u64 sp_fuzz_seed_init_str(sp_str_t seed) {
  if (sp_str_empty(seed)) {
    seed = sp_os_env_get(sp_str_lit("SPN_TEST_SEED"));
  }

  if (!sp_str_empty(seed)) {
    sp_fuzz_seed = sp_fuzz_parse_u64(seed);
  }
  else {
    sp_tm_epoch_t now = sp_tm_now_epoch();
    sp_fuzz_prng_t entropy = { .state = now.s ^ ((u64)now.ns << 32) };
    sp_fuzz_seed = sp_fuzz_next(&entropy);
  }

  sp_io_stream_writer_t out = sp_io_get_std_out();
  sp_fmt_io(&out.base, "SPN_TEST_SEED=0x{}\n", sp_fmt_u64_custom(sp_fuzz_seed, sp_fuzz_fmt_hex));
  return sp_fuzz_seed;
}

u64 sp_fuzz_seed_init(void) {
  return sp_fuzz_seed_init_str(sp_str_lit(""));
}

// Derive the stream from the seed and the name set, not from a shared stream
// position, so a failure reproduces under --filter
sp_fuzz_prng_t sp_fuzz_stream(sp_da(sp_str_t) names) {
  sp_fuzz_prng_t prng = { .state = sp_fuzz_seed };
  sp_da_for(names, it) {
    prng.state = sp_hash_bytes(names[it].data, names[it].len, prng.state);
  }
  return prng;
}

sp_fuzz_prng_t sp_fuzz_iter(sp_fuzz_prng_t base, u64 iter) {
  return (sp_fuzz_prng_t) { .state = base.state ^ ((iter + 1) * 0x9e3779b97f4a7c15u) };
}

sp_fuzz_opts_t sp_fuzz_opts(u64 default_iters) {
  sp_fuzz_opts_t opts = { .iters = default_iters, .only = -1 };

  sp_str_t iters_env = sp_os_env_get(sp_str_lit("SPN_FUZZ_ITERS"));
  if (!sp_str_empty(iters_env)) {
    opts.iters = sp_fuzz_parse_u64(iters_env);
  }

  sp_str_t only_env = sp_os_env_get(sp_str_lit("SPN_FUZZ_ITER"));
  if (!sp_str_empty(only_env)) {
    opts.only = (s64)sp_fuzz_parse_u64(only_env);
    opts.iters = (u64)opts.only + 1;
  }

  return opts;
}

sp_intern_t* sp_fuzz_perturbed_intern(sp_fuzz_prng_t* prng, sp_da(sp_str_t) names) {
  sp_intern_t* intern = sp_intern_new(sp_mem_os_new());

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_str_t) shuffled = sp_da_new(scratch.mem, sp_str_t);
  sp_da_for(names, it) {
    sp_da_push(shuffled, names[it]);
  }
  sp_fuzz_shuffle(prng, shuffled, sp_da_size(shuffled), sizeof(sp_str_t));

  u32 junk = 0;
  sp_da_for(shuffled, it) {
    while (sp_fuzz_chance(prng, 1, 2)) {
      sp_intern_get_or_insert(intern, sp_fmt(scratch.mem, "#{}", sp_fmt_uint(junk++)).value);
    }
    if (sp_fuzz_chance(prng, 1, 2)) {
      sp_intern_get_or_insert(intern, shuffled[it]);
    }
  }

  sp_mem_end_scratch(scratch);
  return intern;
}
