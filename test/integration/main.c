#define SP_IMPLEMENTATION
#include "sp.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"
#include "action.h"

UTEST_MAIN()

#define uf utest_fixture

#define r(str) str "\n"

typedef struct {
  sp_str_t project;
} runner_t;

struct spn_init {
  struct {
    sp_str_t spn;
  } paths;
  sp_test_file_manager_t file_manager;
  runner_t runner;
};

sp_str_t spn_test_get_project_path(runner_t* runner, sp_str_t relative_path) {
  return sp_fs_join_path(runner->project, relative_path);
}

void run_test(s32* utest_result, struct spn_init* fixture, test_t test) {
  sp_io_writer_t log = sp_io_writer_from_file(sp_str_lit("integration.log"), SP_IO_WRITE_MODE_OVERWRITE);

  sp_for(it, test.num_actions) {
    action_t action = test.actions[it];

    switch (action.kind) {
      case SPN_TEST_ACTION_NONE: {
        break;
      }
      case SPN_TEST_ACTION_CREATE_FILE: {
        sp_str_t path = spn_test_get_project_path(&fixture->runner, action.create.file);
        sp_test_file_create_ex((sp_test_file_config_t) {
          .path = path,
          .content = action.create.content,
        });
        break;
      }
      case SPN_TEST_ACTION_SUBPROCESS: {
        sp_ps_config_t config = action.process.config;
        if (sp_str_empty(config.cwd)) {
          config.cwd = fixture->runner.project;
        }

        sp_ps_output_t output = sp_ps_run(config);
        EXPECT_EQ(action.process.expect_exit_code, output.status.exit_code);
        break;
      }
      case SPN_TEST_ACTION_VERIFY_EXISTS: {
        sp_str_t path = spn_test_get_project_path(&fixture->runner, action.verify_exists.file);
        EXPECT_TRUE(sp_fs_exists(path));
        break;
      }
      case SPN_TEST_ACTION_VERIFY_CONTENT: {
        sp_str_t path = spn_test_get_project_path(&fixture->runner, action.verify_content.file);
        sp_str_t content = sp_io_read_file(path);
        EXPECT_TRUE(sp_str_equal(content, action.verify_content.content));
        break;
      }
    }
  }
}

UTEST_F_SETUP(spn_init) {

  // for now, this is OK
  uf->paths.spn = sp_str_lit("/home/spader/.local/bin/bspn");

  // uf->paths.spn = sp_format(
  //   "{}/{}",
  //   SP_FMT_STR(sp_fs_get_exe_path()),
  //   SP_FMT_CSTR("spn")
  // );

  sp_test_file_manager_init(&uf->file_manager);
  uf->runner.project = sp_test_file_path(&uf->file_manager, sp_str_lit("build_smoke_bare"));
  sp_fs_create_dir(uf->runner.project);
}

UTEST_F_TEARDOWN(spn_init) {
  sp_test_file_manager_cleanup(&uf->file_manager);
}

UTEST_F(spn_init, build_smoke_bare) {
  run_test(utest_result, uf, (test_t) {
    .num_actions = 4,
    .actions = {
      {
        .kind = SPN_TEST_ACTION_CREATE_FILE,
        .create = {
          .file = sp_str_lit("spn.toml"),
          .content = sp_str_lit(
            r("[package]")
            r("name = \"smoke\"")
            r("version = \"0.1.0\"")
            r("")
            r("[[bin]]")
            r("name = \"smoke\"")
            r("source = [\"main.c\"]")
          )
        }
      },
      {
        .kind = SPN_TEST_ACTION_CREATE_FILE,
        .create = {
          .file = sp_str_lit("main.c"),
          .content = sp_str_lit(
            r("#include <stdio.h>")
            r("")
            r("int main() {")
            r("  puts(\"ok\");")
            r("  return 0;")
            r("}")
          )
        }
      },
      {
        .kind = SPN_TEST_ACTION_SUBPROCESS,
        .process = {
          .config = {
            .command = uf->paths.spn,
            .args = {
              sp_str_lit("build"),
              sp_str_lit("--profile"),
              sp_str_lit("debug"),
            }
          },
          .expect_exit_code = 0,
        }
      },
      {
        .kind = SPN_TEST_ACTION_VERIFY_EXISTS,
        .verify_exists = {
          .file = sp_str_lit("build/debug/store/bin/smoke"),
        }
      },
    }
  });
}
