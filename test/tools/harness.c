#include "test.h"
#if !defined(_WIN32)
  #include <unistd.h>
#endif
#include "sp/macro.h"

static ctx_t g_ctx = SP_ZERO_INITIALIZE();

ctx_t* ctx_get() {
  return &g_ctx;
}

ctx_paths_t ctx_get_paths(ctx_t* ctx) {
  ctx_paths_t paths = sp_zero;

  paths.repo = sp_fs_get_exe_path(spn_allocator);
  while (true) {
    sp_assert(!sp_str_empty(paths.repo));
    sp_str_t stem = sp_fs_get_stem(paths.repo);
    if (sp_str_equal(stem, strl("spn"))) {
      break;
    }
    paths.repo = sp_fs_parent_path(paths.repo);
  }

  paths.test.dir = sp_fs_join_path(spn_allocator, paths.repo, strl("test"));
  paths.test.fixtures = sp_fs_join_path(spn_allocator, paths.test.dir, strl("fixtures"));

  return paths;
}

static sp_str_t get_run_tmpdir() {
  sp_str_t tmp = sp_fs_normalize_path(spn_allocator, sp_os_env_get(sp_str_lit("SPN_TEST_TMP")));
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

  tmp = sp_fs_canonicalize_path(spn_allocator, tmp);

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t timestamp = sp_tm_epoch_to_iso8601(spn_allocator, now);
#ifdef _WIN32
  u32 pid = (u32)GetCurrentProcessId();
#else
  u32 pid = (u32)getpid();
#endif
  sp_str_t dirname = sp_format("{}-{}", SP_FMT_STR(sp_str_replace_c8(spn_allocator, timestamp, ':', '-')), SP_FMT_U32(pid));
  return sp_fs_join_path(spn_allocator, tmp, dirname);
}

void ctx_init(ctx_t* ctx) {
  ctx->arena = sp_mem_arena_new(spn_allocator);

  sp_context_push_arena(ctx->arena); {
    tmpfs_set_top_level(get_run_tmpdir());
    tmpfs_init_named(&ctx->fs, "index");
  }
  sp_context_pop();
}

void ctx_deinit(ctx_t* harness) {
  sp_mem_arena_destroy(harness->arena);
  *harness = SP_ZERO_STRUCT(ctx_t);
}

bool str_equal(sp_str_t a, sp_str_t b) {
  if (!a.len && !b.len) return true;
  return sp_str_equal(a, b);
}
