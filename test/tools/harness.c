#include "test.h"
#if !defined(_WIN32)
  #include <unistd.h>
#endif
#include "sp/macro.h"

static ctx_t g_ctx = sp_zero;

ctx_t* ctx_get() {
  return &g_ctx;
}

ctx_paths_t ctx_get_paths(ctx_t* ctx) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx->arena);
  ctx_paths_t paths = sp_zero;

  paths.repo = sp_fs_get_exe_path(mem);
  while (true) {
    sp_assert(!sp_str_empty(paths.repo));
    sp_str_t stem = sp_fs_get_stem(paths.repo);
    if (sp_str_equal(stem, strl("spn"))) {
      break;
    }
    paths.repo = sp_fs_parent_path(paths.repo);
  }

  paths.test.dir = sp_fs_join_path(mem, paths.repo, strl("test"));
  paths.test.fixtures = sp_fs_join_path(mem, paths.test.dir, strl("fixtures"));

  return paths;
}

static sp_str_t get_run_tmpdir(sp_mem_t mem) {
  sp_str_t tmp = sp_fs_normalize_path(mem, sp_os_env_get(sp_str_lit("SPN_TEST_TMP")));
  if (sp_str_empty(tmp)) {
    tmp = sp_str_lit(".tmp");
  }

  sp_str_t parent = sp_fs_parent_path(tmp);
  if (!sp_str_empty(parent) && !sp_fs_exists(parent)) {
    sp_fs_create_dir(parent);
  }

  if (!sp_fs_exists(tmp)) {
    sp_fs_create_dir(tmp);
  }

  tmp = sp_fs_canonicalize_path(mem, tmp);

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t timestamp = sp_tm_epoch_to_iso8601(mem, now);
#ifdef _WIN32
  u32 pid = (u32)GetCurrentProcessId();
#else
  u32 pid = (u32)getpid();
#endif
  sp_str_t dirname = sp_fmt(mem, "{}-{}", sp_fmt_str(sp_str_replace_c8(mem, timestamp, ':', '-')), sp_fmt_uint(pid)).value;
  return sp_fs_join_path(mem, tmp, dirname);
}

void ctx_init(ctx_t* ctx) {
  ctx->arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx->arena);

  tmpfs_set_top_level(get_run_tmpdir(mem));
  tmpfs_init_named(&ctx->fs, "index");
}

void ctx_deinit(ctx_t* harness) {
  sp_mem_arena_destroy(harness->arena);
  *harness = sp_zero_s(ctx_t);
}

bool str_equal(sp_str_t a, sp_str_t b) {
  if (!a.len && !b.len) return true;
  return sp_str_equal(a, b);
}
