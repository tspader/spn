#include "spn_test.h"

#include "atomic_file/atomic_file.h"
#include "fs/fs.h"


#define FS_LOCK_MAX_SLOTS 4
#define FS_LOCK_MAX_OPS 8

typedef enum {
  FS_LOCK_OP_NONE,
  FS_LOCK_OP_ACQUIRE,
  FS_LOCK_OP_TRY,
  FS_LOCK_OP_RELEASE,
} fs_lock_op_kind_t;

typedef struct {
  bool acquired;
} fs_lock_expect_t;

typedef struct {
  fs_lock_op_kind_t kind;
  u32 slot;
  const c8* path;
  fs_lock_expect_t expect;
} fs_lock_op_t;

typedef struct {
  const c8* name;
  fs_lock_op_t ops [FS_LOCK_MAX_OPS];
} fs_lock_test_t;

static const fs_lock_test_t fs_lock_tests [] = {
  {
    .name = "acquire_release",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .path = "a.lock" },
      { .kind = FS_LOCK_OP_RELEASE },
    },
  },
  {
    .name = "try_uncontended",
    .ops = {
      { .kind = FS_LOCK_OP_TRY, .path = "a.lock", .expect.acquired = true },
    },
  },
  {
    .name = "try_contended",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .path = "a.lock" },
      { .kind = FS_LOCK_OP_TRY, .slot = 1, .path = "a.lock" },
      { .kind = FS_LOCK_OP_RELEASE },
      { .kind = FS_LOCK_OP_TRY, .slot = 1, .path = "a.lock", .expect.acquired = true },
    },
  },
  {
    .name = "distinct_paths",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .path = "a.lock" },
      { .kind = FS_LOCK_OP_TRY, .slot = 1, .path = "b.lock", .expect.acquired = true },
    },
  },
  {
    .name = "release_unheld",
    .ops = {
      { .kind = FS_LOCK_OP_RELEASE },
      { .kind = FS_LOCK_OP_TRY, .path = "a.lock", .expect.acquired = true },
    },
  },
  {
    .name = "reacquire_same_slot",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .path = "a.lock" },
      { .kind = FS_LOCK_OP_RELEASE },
      { .kind = FS_LOCK_OP_ACQUIRE, .path = "a.lock" },
      { .kind = FS_LOCK_OP_RELEASE },
    },
  },
};

sp_test_each(fs_lock, ops, fs_lock_test_t, fs_lock_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t sandbox = sp_test_dir(t);

  sp_fs_lock_t slots [FS_LOCK_MAX_SLOTS] = sp_zero;

  sp_carr_for(it->ops, i) {
    fs_lock_op_t op = it->ops[i];
    if (op.kind == FS_LOCK_OP_NONE) {
      break;
    }

    sp_fs_lock_t* lock = &slots[op.slot];
    sp_str_t path = op.path
      ? sp_fs_join_path(mem, sandbox, sp_cstr_as_str(op.path))
      : sp_str_lit("");

    switch (op.kind) {
      case FS_LOCK_OP_NONE: sp_unreachable_case();
      case FS_LOCK_OP_ACQUIRE: {
        sp_expect_ok(t, sp_fs_lock_acquire(lock, path));
        sp_expect(t, lock->held);
        break;
      }
      case FS_LOCK_OP_TRY: {
        bool acquired = false;
        sp_expect_ok(t, sp_fs_lock_try_acquire(lock, path, &acquired));
        sp_expect_eq(t, acquired, op.expect.acquired);
        sp_expect_eq(t, lock->held, op.expect.acquired);
        break;
      }
      case FS_LOCK_OP_RELEASE: {
        sp_expect_ok(t, sp_fs_lock_release(lock));
        sp_expect(t, !lock->held);
        break;
      }
    }
  }

  sp_carr_for(slots, i) {
    sp_fs_lock_release(&slots[i]);
  }

  return SP_OK;
}

typedef struct {
  sp_str_t path;
  sp_atomic_s32_t acquired;
} fs_lock_waiter_t;

static s32 fs_lock_waiter_fn(void* user_data) {
  fs_lock_waiter_t* waiter = (fs_lock_waiter_t*)user_data;

  sp_fs_lock_t lock = sp_zero;
  if (sp_fs_lock_acquire(&lock, waiter->path)) {
    return 1;
  }

  sp_atomic_s32_store(&waiter->acquired, 1, SP_ATOMIC_SEQ_CST);
  sp_fs_lock_release(&lock);
  return 0;
}

sp_test(fs_lock, acquire_blocks_until_release, .serial = true) {
  fs_lock_waiter_t waiter = {
    .path = sp_fs_join_path(sp_test_arena(t), sp_test_dir(t), sp_str_lit("a.lock")),
  };

  sp_fs_lock_t lock = sp_zero;
  sp_must_ok(t, sp_fs_lock_acquire(&lock, waiter.path));

  sp_thread_t thread;
  sp_thread_init(&thread, fs_lock_waiter_fn, &waiter);

  sp_os_sleep_ms(50);
  sp_expect_eq(t, sp_atomic_s32_load(&waiter.acquired, SP_ATOMIC_SEQ_CST), 0);

  sp_fs_lock_release(&lock);
  sp_thread_join(&thread);
  sp_expect_eq(t, sp_atomic_s32_load(&waiter.acquired, SP_ATOMIC_SEQ_CST), 1);

  return SP_OK;
}

sp_test(fs_staging, claims_distinct_dirs) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("checkout"));

  sp_str_t a = sp_zero;
  sp_str_t b = sp_zero;
  sp_must_ok(t, sp_fs_staging_dir(mem, path, sp_str_lit("tmp"), &a));
  sp_must_ok(t, sp_fs_staging_dir(mem, path, sp_str_lit("tmp"), &b));
  sp_expect(t, !sp_str_equal(a, b));
  sp_expect(t, sp_fs_is_dir(a));
  sp_expect(t, sp_fs_is_dir(b));
  sp_expect(t, sp_str_starts_with(a, path));
  sp_expect(t, sp_str_ends_with(a, sp_str_lit("tmp")));

  return SP_OK;
}

sp_test(fs_staging, creates_parent) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("nested")), sp_str_lit("checkout"));

  sp_str_t dir = sp_zero;
  sp_must_ok(t, sp_fs_staging_dir(mem, path, sp_str_lit("tmp"), &dir));
  sp_expect(t, sp_fs_is_dir(dir));

  return SP_OK;
}

sp_test(fs_staging, fails_when_parent_is_file) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t file = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("occupied"));
  sp_fs_create_file_str(file, sp_str_lit("x"));
  sp_str_t path = sp_fs_join_path(mem, file, sp_str_lit("checkout"));

  sp_str_t dir = sp_zero;
  sp_expect_ne(t, sp_fs_staging_dir(mem, path, sp_str_lit("tmp"), &dir), SP_OK);
  sp_expect(t, sp_str_empty(dir));

  return SP_OK;
}

sp_test(fs_append, creates) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("rc"));

  sp_must_ok(t, sp_fs_append(path, sp_str_lit("\nL\n")));

  sp_str_t content = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, path, &content));
  sp_expect_str_eq_c(t, content, "\nL\n");
  return SP_OK;
}

sp_test(fs_append, appends) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("rc"));
  sp_must_ok(t, sp_fs_create_file_cstr(path, "A\n"));

  sp_must_ok(t, sp_fs_append(path, sp_str_lit("\nL\n")));

  sp_str_t content = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, path, &content));
  sp_expect_str_eq_c(t, content, "A\n\nL\n");
  return SP_OK;
}

sp_test(fs_copy_atomic, replaces) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t src = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("src"));
  sp_str_t dst = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("dst"));
  sp_must_ok(t, sp_fs_create_file_cstr(src, "NEW"));
  sp_must_ok(t, sp_fs_create_file_cstr(dst, "OLD"));

  sp_must_ok(t, sp_fs_copy_atomic(dst, src));

  sp_str_t content = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, dst, &content));
  sp_expect_str_eq_c(t, content, "NEW");
  sp_must_ok(t, sp_io_read_file(mem, src, &content));
  sp_expect_str_eq_c(t, content, "NEW");
  return SP_OK;
}

sp_test(fs_copy_atomic, missing_source) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t src = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("absent"));
  sp_str_t dst = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("dst"));

  sp_expect_ne(t, sp_fs_copy_atomic(dst, src), SP_OK);
  sp_expect(t, !sp_fs_exists(dst));
  return SP_OK;
}

sp_test(fs_copy_atomic, preserves_mode) {
  sp_test_skip_on_win32();

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t src = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("src"));
  sp_str_t dst = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("dst"));
  sp_must_ok(t, sp_fs_create_file_cstr(src, "X"));
  sp_ps_output_t chmod = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("chmod"),
    .args = { sp_str_lit("+x"), src },
  });
  sp_must_eq(t, 0, chmod.status.exit_code);

  sp_must_ok(t, sp_fs_copy_atomic(dst, src));

  sp_sys_file_meta_t meta = sp_zero;
  sp_must_ok(t, sp_sys_get_path_metadata_s(sp_sys_get_root(0), dst, &meta));
  sp_expect(t, (meta.raw_attrs & 0111) != 0);
  return SP_OK;
}

sp_test(fs_copy_atomic, busy) {
  sp_test_skip_on_win32();

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_test_dir(t);

  sp_ps_output_t which = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("sh"),
    .args = { sp_str_lit("-c"), sp_str_lit("command -v sleep") },
  });
  sp_must_eq(t, 0, which.status.exit_code);
  sp_str_t sleep_bin = sp_str_trim_right(which.out);

  sp_str_t target = sp_fs_join_path(mem, dir, sp_str_lit("bin/spn"));
  sp_must_ok(t, sp_fs_create_dir(sp_fs_parent_path(target)));
  sp_must_ok(t, sp_fs_copy_file(sleep_bin, target));

  sp_ps_t running = sp_ps_create(mem, (sp_ps_config_t) {
    .command = target,
    .args = { sp_str_lit("30") },
    .io = SP_PS_NO_STDIO,
  });
  sp_must(t, running.os);

  sp_str_t source = sp_fs_join_path(mem, dir, sp_str_lit("src"));
  sp_must_ok(t, sp_fs_create_file_cstr(source, "N"));

  sp_err_t err = sp_fs_copy_atomic(target, source);

  sp_ps_kill(&running);
  sp_ps_wait(&running);
  sp_ps_free(&running);

  sp_must_ok(t, err);
  sp_str_t content = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, target, &content));
  sp_expect_str_eq_c(t, content, "N");
  return SP_OK;
}
