#include "test.h"

void tmpfs_init(tmpfs_t* fs) {
  sp_str_t tmp = sp_os_get_env_as_path(sp_str_lit("TMPDIR"));
  if (sp_str_empty(tmp)) {
    tmp = sp_str_lit("/tmp");
  }

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t root = sp_format(
    "{}/spn-test-{}-{}",
    SP_FMT_STR(tmp),
    SP_FMT_U64(now.s),
    SP_FMT_U32(now.ns)
  );

  SP_ASSERT(!sp_fs_exists(root));
  sp_fs_create_dir(root);
  fs->root = root;
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
