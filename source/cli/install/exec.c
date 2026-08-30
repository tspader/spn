#include "install/install.h"

#include "atomic_file/atomic_file.h"

static sp_err_t install_exe(spn_install_action_t* action) {
  sp_err_t err = SP_OK;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_sys_file_meta_t meta = sp_zero;
  sp_try_goto(sp_sys_get_path_metadata_s(sp_sys_get_root(0), action->src, &meta), err, done);

  sp_str_t content = sp_zero;
  sp_try_goto(sp_io_read_file(scratch.mem, action->src, &content), err, done);

  sp_fs_atomic_t file = sp_zero;
  sp_try_goto(sp_fs_atomic_open(&file, action->path), err, done);
  err = sp_io_write_all(sp_fs_atomic_writer(&file), content.data, content.len, SP_NULLPTR);
  if (!err) {
    err = sp_sys_chmod_s(sp_sys_get_root(0), file.temp, &meta);
  }
  if (err) {
    sp_fs_atomic_abort(&file);
    goto done;
  }
  err = sp_fs_atomic_commit(&file, SP_FS_ATOMIC_REPLACE);

done:
  sp_mem_end_scratch(scratch);
  return err;
}

static sp_err_t append_line(spn_install_action_t* action) {
  sp_sys_fd_t fd = SP_SYS_INVALID_FD;
  sp_try(sp_sys_open_s(sp_sys_get_root(0), action->path, SP_SYS_OPEN_MODE_WO, SP_SYS_OPEN_CREATE | SP_SYS_OPEN_APPEND, &fd));
  sp_io_file_writer_t io = sp_zero;
  sp_try(sp_io_file_writer_from_fd(&io, fd, SP_IO_CLOSE_MODE_AUTO));
  sp_err_t written = sp_io_file_writer_seek(&io, 0, SP_IO_SEEK_END, SP_NULLPTR);
  if (!written) {
    written = sp_io_write_all(&io.base, action->text.data, action->text.len, SP_NULLPTR);
  }
  sp_err_t closed = sp_io_file_writer_close(&io);
  return written ? written : closed;
}

#if defined(SP_WIN32)
static sp_err_t set_user_path(spn_install_action_t* action) {
  sp_err_t err = SP_ERR_SYS;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_wide_str_t value = sp_zero;
  sp_try_goto(sp_wtf8_to_wtf16(scratch.mem, action->text, &value), err, done);

  HKEY key = SP_NULLPTR;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &key)) {
    goto done;
  }
  DWORD kind = action->expand ? REG_EXPAND_SZ : REG_SZ;
  if (!RegSetValueExW(key, L"Path", 0, kind, (const BYTE*)value.data, (DWORD)((value.len + 1) * sizeof(u16)))) {
    err = SP_OK;
  }
  RegCloseKey(key);
  if (!err) {
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, SP_NULLPTR);
  }

done:
  sp_mem_end_scratch(scratch);
  return err;
}
#else
static sp_err_t set_user_path(spn_install_action_t* action) {
  SP_UNUSED(action);
  SP_UNREACHABLE_RETURN(SP_ERR);
}
#endif

static sp_err_t apply(spn_install_action_t* action) {
  switch (action->kind) {
    case SPN_INSTALL_ACTION_NONE: return SP_ERR;
    case SPN_INSTALL_ACTION_CREATE_DIR: return sp_fs_create_dir(action->path);
    case SPN_INSTALL_ACTION_INSTALL_EXE: return install_exe(action);
    case SPN_INSTALL_ACTION_WRITE_FILE: return sp_fs_write_atomic(action->path, action->text);
    case SPN_INSTALL_ACTION_APPEND_LINE: return append_line(action);
    case SPN_INSTALL_ACTION_SET_USER_PATH: return set_user_path(action);
  }
  return SP_ERR;
}

spn_install_result_t spn_install_execute(spn_install_plan_t* plan) {
  spn_install_result_t result = sp_zero;

  sp_for(it, plan->num_install) {
    result.err = apply(&plan->install[it]);
    if (result.err) {
      result.failed = plan->install[it];
      return result;
    }
  }
  sp_for(it, plan->num_path) {
    if (apply(&plan->path[it])) {
      result.stuck[result.num_stuck++] = (u32)it;
    }
  }
  return result;
}
