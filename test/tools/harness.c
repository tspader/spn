#include "test.h"
#include "sp/macro.h"

static ctx_t g_ctx = SP_ZERO_INITIALIZE();

ctx_t* ctx_get() {
  return &g_ctx;
}

ctx_paths_t ctx_get_paths(ctx_t* ctx) {
  ctx_paths_t paths = sp_zero;

  paths.repo = sp_fs_get_exe_path();
  while (true) {
    sp_assert(!sp_str_empty(paths.repo));
    sp_str_t stem = sp_fs_get_stem(paths.repo);
    if (sp_str_equal(stem, strl("spn"))) {
      break;
    }
    paths.repo = sp_fs_parent_path(paths.repo);
  }

  paths.test.dir = sp_fs_join_path(paths.repo, strl("test"));
  paths.test.resolver.dir = sp_fs_join_path(paths.test.dir, strl("core/resolver"));
  paths.test.resolver.fixtures = sp_fs_join_path(paths.test.resolver.dir, strl("fixtures"));

  return paths;
}

static sp_str_t get_run_tmpdir() {
  sp_str_t tmp = sp_fs_normalize_path(sp_os_env_get(sp_str_lit("SPN_TEST_TMP")));
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

  tmp = sp_fs_canonicalize_path(tmp);

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t timestamp = sp_tm_epoch_to_iso8601(now);
#ifdef _WIN32
  u32 pid = (u32)GetCurrentProcessId();
#else
  u32 pid = (u32)sp_sys_getpid();
#endif
  sp_str_t dirname = sp_format("{}-{}", SP_FMT_STR(sp_str_replace_c8(timestamp, ':', '-')), SP_FMT_U32(pid));
  return sp_fs_join_path(tmp, dirname);
}

void ctx_init(ctx_t* ctx) {
  ctx->arena = sp_mem_arena_new(4096);

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
