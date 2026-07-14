#define SP_IMPLEMENTATION
#define SP_TEST_IMPLEMENTATION
#include "sp.h"
#include "sp/fs.h"
#include "sp_test.h"
#include "utest.h"

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

struct fs_lock {
  sp_test_file_manager_t file_manager;
};

UTEST_F_SETUP(fs_lock) {
  sp_test_file_manager_init(&uf->file_manager);
}

UTEST_F_TEARDOWN(fs_lock) {
  sp_test_file_manager_cleanup(&uf->file_manager);
}

#define FS_LOCK_MAX_SLOTS 4
#define FS_LOCK_MAX_OPS 8

typedef enum {
  FS_LOCK_OP_NONE,
  FS_LOCK_OP_ACQUIRE,
  FS_LOCK_OP_TRY,
  FS_LOCK_OP_RELEASE,
} fs_lock_op_kind_t;

typedef struct {
  fs_lock_op_kind_t kind;
  u32 slot;
  const c8* path;
  struct {
    bool acquired;
  } expect;
} fs_lock_op_t;

typedef struct {
  const c8* label;
  fs_lock_op_t ops[FS_LOCK_MAX_OPS];
} fs_lock_test_t;

static void run_fs_lock_test(s32* utest_result, sp_test_file_manager_t* fm, fs_lock_test_t t) {
  sp_str_t sandbox = sp_test_file_path_c(fm, t.label);
  sp_fs_create_dir(sandbox);

  sp_fs_lock_t slots[FS_LOCK_MAX_SLOTS] = sp_zero;

  sp_carr_for(t.ops, it) {
    fs_lock_op_t op = t.ops[it];
    if (op.kind == FS_LOCK_OP_NONE) {
      break;
    }

    sp_fs_lock_t* lock = &slots[op.slot];
    sp_str_t path = op.path
      ? sp_fs_join_path(fm->mem, sandbox, sp_cstr_as_str(op.path))
      : sp_str_lit("");

    switch (op.kind) {
      case FS_LOCK_OP_NONE: {
        break;
      }
      case FS_LOCK_OP_ACQUIRE: {
        EXPECT_EQ(sp_fs_lock_acquire(lock, path), SP_OK);
        EXPECT_TRUE(lock->held);
        break;
      }
      case FS_LOCK_OP_TRY: {
        bool acquired = false;
        EXPECT_EQ(sp_fs_lock_try_acquire(lock, path, &acquired), SP_OK);
        EXPECT_EQ(acquired, op.expect.acquired);
        EXPECT_EQ(lock->held, op.expect.acquired);
        break;
      }
      case FS_LOCK_OP_RELEASE: {
        EXPECT_EQ(sp_fs_lock_release(lock), SP_OK);
        EXPECT_FALSE(lock->held);
        break;
      }
    }
  }

  sp_carr_for(slots, it) {
    sp_fs_lock_release(&slots[it]);
  }
}

UTEST_F(fs_lock, acquire_release) {
  run_fs_lock_test(&ur, &uf->file_manager, (fs_lock_test_t) {
    .label = "acquire_release",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .slot = 0, .path = "a.lock" },
      { .kind = FS_LOCK_OP_RELEASE, .slot = 0 },
    },
  });
}

UTEST_F(fs_lock, try_uncontended) {
  run_fs_lock_test(&ur, &uf->file_manager, (fs_lock_test_t) {
    .label = "try_uncontended",
    .ops = {
      { .kind = FS_LOCK_OP_TRY, .slot = 0, .path = "a.lock", .expect.acquired = true },
    },
  });
}

UTEST_F(fs_lock, try_contended) {
  run_fs_lock_test(&ur, &uf->file_manager, (fs_lock_test_t) {
    .label = "try_contended",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .slot = 0, .path = "a.lock" },
      { .kind = FS_LOCK_OP_TRY, .slot = 1, .path = "a.lock", .expect.acquired = false },
      { .kind = FS_LOCK_OP_RELEASE, .slot = 0 },
      { .kind = FS_LOCK_OP_TRY, .slot = 1, .path = "a.lock", .expect.acquired = true },
    },
  });
}

UTEST_F(fs_lock, distinct_paths) {
  run_fs_lock_test(&ur, &uf->file_manager, (fs_lock_test_t) {
    .label = "distinct_paths",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .slot = 0, .path = "a.lock" },
      { .kind = FS_LOCK_OP_TRY, .slot = 1, .path = "b.lock", .expect.acquired = true },
    },
  });
}

UTEST_F(fs_lock, release_unheld) {
  run_fs_lock_test(&ur, &uf->file_manager, (fs_lock_test_t) {
    .label = "release_unheld",
    .ops = {
      { .kind = FS_LOCK_OP_RELEASE, .slot = 0 },
      { .kind = FS_LOCK_OP_TRY, .slot = 0, .path = "a.lock", .expect.acquired = true },
    },
  });
}

UTEST_F(fs_lock, reacquire_same_slot) {
  run_fs_lock_test(&ur, &uf->file_manager, (fs_lock_test_t) {
    .label = "reacquire_same_slot",
    .ops = {
      { .kind = FS_LOCK_OP_ACQUIRE, .slot = 0, .path = "a.lock" },
      { .kind = FS_LOCK_OP_RELEASE, .slot = 0 },
      { .kind = FS_LOCK_OP_ACQUIRE, .slot = 0, .path = "a.lock" },
      { .kind = FS_LOCK_OP_RELEASE, .slot = 0 },
    },
  });
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

  sp_atomic_s32_set(&waiter->acquired, 1);
  sp_fs_lock_release(&lock);
  return 0;
}

UTEST_F(fs_lock, acquire_blocks_until_release) {
  sp_str_t sandbox = sp_test_file_path_c(&uf->file_manager, "acquire_blocks_until_release");
  sp_fs_create_dir(sandbox);

  fs_lock_waiter_t waiter = {
    .path = sp_fs_join_path(uf->file_manager.mem, sandbox, sp_str_lit("a.lock")),
  };

  sp_fs_lock_t lock = sp_zero;
  ASSERT_EQ(sp_fs_lock_acquire(&lock, waiter.path), SP_OK);

  sp_thread_t thread;
  sp_thread_init(&thread, fs_lock_waiter_fn, &waiter);

  sp_os_sleep_ms(50);
  EXPECT_EQ(sp_atomic_s32_get(&waiter.acquired), 0);

  sp_fs_lock_release(&lock);
  sp_thread_join(&thread);
  EXPECT_EQ(sp_atomic_s32_get(&waiter.acquired), 1);
}
