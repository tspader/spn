#include "test.h"
#if !defined(SP_WIN32)
  #include <unistd.h>
#endif

static sp_str_t tmpfs_top_level = sp_zero;

sp_str_t tmpfs_default_top_level() {
  sp_mem_t mem = sp_mem_os_new();

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
#ifdef SP_WIN32
  u32 pid = (u32)GetCurrentProcessId();
#else
  u32 pid = (u32)getpid();
#endif
  sp_str_t dirname = sp_fmt(mem, "{}-{}", sp_fmt_str(sp_str_replace_c8(mem, timestamp, ':', '-')), sp_fmt_uint(pid)).value;
  return sp_fs_join_path(mem, tmp, dirname);
}

void tmpfs_set_top_level(sp_str_t root) {
  sp_str_t parent = sp_fs_parent_path(root);
  if (!sp_str_empty(parent) && !sp_fs_exists(parent)) {
    sp_fs_create_dir(parent);
  }

  if (!sp_fs_exists(root)) {
    sp_fs_create_dir(root);
  }

  tmpfs_top_level = sp_fs_canonicalize_path(sp_mem_os_new(), root);
}

void tmpfs_init_named(tmpfs_t* fs, const c8* test) {
  fs->mem = sp_mem_os_new();

  if (sp_str_empty(tmpfs_top_level)) {
    tmpfs_set_top_level(tmpfs_default_top_level());
  }

  sp_str_t test_name = sp_str_view(test);
  if (sp_str_empty(test_name)) {
    test_name = sp_str_lit("tmpfs");
  }

  sp_str_t root = sp_fs_join_path(fs->mem, tmpfs_top_level, test_name);

  SP_ASSERT(!sp_fs_exists(root));
  sp_fs_create_dir(root);
  fs->root = root;
}

void tmpfs_init(tmpfs_t* fs) {
  tmpfs_init_named(fs, "tmpfs");
}

sp_str_t tmpfs_get(tmpfs_t* fs, sp_str_t name) {
  return sp_fs_join_path(fs->mem, fs->root, name);
}

void tmpfs_create(tmpfs_t* fs, sp_str_t relative, sp_str_t content) {
  sp_str_t path = tmpfs_get(fs, relative);
  sp_fs_create_dir(sp_fs_parent_path(path));

  sp_fs_remove_file(path);

  sp_io_file_writer_t f = sp_zero;
  sp_io_file_writer_from_path(&f, path);
  if (!sp_str_empty(content)) {
    sp_io_write(&f.base, content.data, content.len, SP_NULLPTR);
  }
  sp_io_file_writer_close(&f);
}

sp_str_t tmpfs_touch(tmpfs_t* fs, sp_str_t relative) {
  sp_str_t path = tmpfs_get(fs, relative);
  tmpfs_create(fs, relative, sp_str_lit(""));

  return path;
}

void tmpfs_deinit(tmpfs_t* fs) {
  if (sp_str_empty(fs->root)) {
    return;
  }

  sp_fs_remove_dir(fs->root);
}
