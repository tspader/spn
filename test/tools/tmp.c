#include "test.h"

static sp_str_t tmpfs_top_level = SP_ZERO_INITIALIZE();

sp_str_t tmpfs_default_top_level() {
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

void tmpfs_set_top_level(sp_str_t root) {
  sp_str_t parent = sp_fs_parent_path(root);
  if (!sp_str_empty(parent) && !sp_fs_exists(parent)) {
    sp_fs_create_dir(parent);
  }

  if (!sp_fs_exists(root)) {
    sp_fs_create_dir(root);
  }

  tmpfs_top_level = sp_fs_canonicalize_path(root);
}

void tmpfs_init_named(tmpfs_t* fs, const c8* test) {
  if (sp_str_empty(tmpfs_top_level)) {
    tmpfs_set_top_level(tmpfs_default_top_level());
  }

  sp_str_t test_name = sp_str_view(test);
  if (sp_str_empty(test_name)) {
    test_name = sp_str_lit("tmpfs");
  }

  sp_str_t root = sp_fs_join_path(tmpfs_top_level, test_name);

  SP_ASSERT(!sp_fs_exists(root));
  sp_fs_create_dir(root);
  fs->root = root;
}

void tmpfs_init(tmpfs_t* fs) {
  tmpfs_init_named(fs, "tmpfs");
}

sp_str_t tmpfs_get(tmpfs_t* fs, sp_str_t name) {
  return sp_fs_join_path(fs->root, name);
}

void tmpfs_create(tmpfs_t* fs, sp_str_t relative, sp_str_t content) {
  sp_str_t path = tmpfs_get(fs, relative);
  sp_fs_create_dir(sp_fs_parent_path(path));

  sp_fs_remove_file(path);

  sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
  SP_ASSERT(io.file.fd != 0);

  if (!sp_str_empty(content)) {
    u64 bytes_written = sp_io_write(&io, content.data, content.len);
    SP_ASSERT(bytes_written == content.len);
  }

  sp_io_writer_close(&io);
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
