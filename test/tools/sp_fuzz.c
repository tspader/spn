#include "sp_fuzz.h"

#include "intern/intern.h"

static u64 sp_fuzz_seed;
static sp_str_t sp_fuzz_render;

u64 sp_fuzz_seed_get(void) {
  return sp_fuzz_seed;
}

sp_str_t sp_fuzz_render_path(void) {
  return sp_fuzz_render;
}

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

typedef struct {
  const sp_fuzz_desc_t* desc;
  s64 iters;
  s64 iter;
  const c8* seed;
  const c8* render;
  bool keep_going;
  s32 status;
} sp_fuzz_cli_t;

static sp_cli_result_t sp_fuzz_cli_run(sp_cli_t* cli) {
  sp_fuzz_cli_t* config = (sp_fuzz_cli_t*)cli->user_data;
  const sp_fuzz_desc_t* desc = config->desc;

  sp_fuzz_seed_init_str(config->seed ? sp_cstr_as_str(config->seed) : sp_str_lit(""));

  sp_fuzz_opts_t opts = sp_fuzz_opts(desc->iters);
  if (config->iters >= 0) {
    opts.iters = (u64)config->iters;
    opts.only = -1;
  }
  if (config->iter >= 0) {
    opts.only = config->iter;
    opts.iters = (u64)config->iter + 1;
  }
  bool keep_going = config->keep_going || !sp_str_empty(sp_os_env_get(sp_str_lit("SPN_FUZZ_KEEP_GOING")));
  sp_fuzz_render = config->render ? sp_cstr_as_str(config->render) : sp_os_env_get(sp_str_lit("SPN_FUZZ_RENDER"));

  sp_mem_t mem = sp_mem_os_new();
  sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
  sp_da_push(names, sp_str_view(desc->name));
  sp_fuzz_prng_t base = sp_fuzz_stream(names);

  sp_io_stream_writer_t out = sp_io_get_std_out();
  u64* failures = sp_alloc_n(mem, u64, desc->errs);
  sp_mem_zero(failures, desc->errs * sizeof(u64));
  u64 failed = 0;

  for (u64 iter = 0; iter < opts.iters; iter++) {
    if (opts.only >= 0 && iter != (u64)opts.only) continue;

    sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
    u32 err = desc->run(sp_mem_arena_as_allocator(arena), sp_fuzz_iter(base, iter), iter);
    sp_mem_arena_destroy(arena);
    if (!err) continue;

    failed++;
    if (err < desc->errs) {
      failures[err]++;
    }
    sp_fmt_io(&out.base, "fuzz: {} (iter {})\n", sp_fmt_str(desc->err_str(err)), sp_fmt_uint(iter));
    if (!keep_going) {
      config->status = (s32)err;
      return SP_CLI_OK;
    }
  }

  if (failed) {
    sp_fmt_io(&out.base, "fuzz: {} of {} iterations failed\n", sp_fmt_uint(failed), sp_fmt_uint(opts.iters));
    for (u32 it = 0; it < desc->errs; it++) {
      if (!failures[it]) continue;
      sp_fmt_io(&out.base, "  {}: {}\n", sp_fmt_uint(failures[it]), sp_fmt_str(desc->err_str(it)));
    }
    config->status = 1;
  }

  return SP_CLI_OK;
}

s32 sp_fuzz_main(s32 num_args, c8** args, const sp_fuzz_desc_t* desc) {
  sp_fuzz_cli_t config = {
    .desc = desc,
    .iters = -1,
    .iter = -1,
  };

  sp_cli_cmd_t root = {
    .name = desc->name,
    .summary = desc->summary,
    .opts = {
      {
        .brief = "n",
        .name = "iters",
        .kind = SP_CLI_OPT_S64,
        .summary = "Number of iterations to run",
        .placeholder = "N",
        .ptr = &config.iters,
      },
      {
        .brief = "i",
        .name = "iter",
        .kind = SP_CLI_OPT_S64,
        .summary = "Run a single iteration, e.g. to replay a dumped failure",
        .placeholder = "ITER",
        .ptr = &config.iter,
      },
      {
        .brief = "s",
        .name = "seed",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "PRNG seed, decimal or 0x-hex; random when unset",
        .placeholder = "SEED",
        .ptr = &config.seed,
      },
      {
        .brief = "k",
        .name = "keep-going",
        .kind = SP_CLI_OPT_BOOLEAN,
        .summary = "Run every iteration instead of stopping at the first failure",
        .ptr = &config.keep_going,
      },
      {
        .brief = "r",
        .name = "render",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "Render each iteration into DIR/NNN/ with everything needed to replay it",
        .placeholder = "DIR",
        .ptr = &config.render,
      },
    },
    .env = {
      { .name = "SPN_TEST_SEED",       .kind = SP_CLI_OPT_CSTR, .summary = "Same as --seed, which wins when both are set" },
      { .name = "SPN_FUZZ_ITERS",      .kind = SP_CLI_OPT_CSTR, .summary = "Same as --iters, which wins when both are set" },
      { .name = "SPN_FUZZ_ITER",       .kind = SP_CLI_OPT_CSTR, .summary = "Same as --iter, which wins when both are set" },
      { .name = "SPN_FUZZ_KEEP_GOING", .kind = SP_CLI_OPT_CSTR, .summary = "Same as --keep-going" },
      { .name = "SPN_FUZZ_RENDER",     .kind = SP_CLI_OPT_CSTR, .summary = "Same as --render, which wins when both are set" },
    },
    .handler = sp_fuzz_cli_run,
  };

  switch (sp_cli_run((sp_cli_desc_t) {
    .root = &root,
    .args = (const c8**)args,
    .num_args = num_args,
    .user_data = &config,
  })) {
    case SP_CLI_OK:       return config.status;
    case SP_CLI_HELP:
    case SP_CLI_CONTINUE: return 0;
    case SP_CLI_ERR:      return 1;
  }
  sp_unreachable_return(1);
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
