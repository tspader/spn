#include "sp.h"
#include "utest.h"
#include "test.h"

#include "index/index.h"
#include "sp/str.h"

UTEST_EMPTY_FIXTURE(index_publish)

typedef struct {
  const c8* path;
  const c8* lines[8];
} publish_file_t;

typedef struct {
  const c8* name;

  struct {
    publish_file_t files[4];
  } fixture;

  struct {
    const c8* namespace;
    const c8* name;
    spn_semver_t version;
    struct { const c8* url; const c8* rev; } source;
  } release;

  struct {
    spn_err_t result;
    u32 lines;
  } expect;
} publish_case_t;

static void write_publish_fixtures(ctx_t* harness, const c8* name, const publish_file_t files[4]) {
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);
  sp_str_t prefix = sp_str_view(name);

  sp_for(it, 4) {
    publish_file_t file = files[it];
    if (!file.path) { break; }

    sp_io_dyn_mem_writer_t builder = sp_zero;
    sp_io_dyn_mem_writer_init(mem, &builder);
    sp_for(line_it, sp_carr_len(file.lines)) {
      if (!file.lines[line_it]) { break; }
      sp_io_write_str(&builder.base, sp_str_view(file.lines[line_it]), SP_NULLPTR);
      sp_io_write_c8(&builder.base, '\n');
    }

    tmpfs_create(
      &harness->fs,
      sp_fs_join_path(mem, prefix, sp_fs_join_path(mem, sp_str_lit("index"), sp_str_view(file.path))),
      sp_io_dyn_mem_writer_take_str(&builder)
    );
  }
}

static void run_publish_case(s32* utest_result, publish_case_t c) {
  ctx_t* harness = ctx_get();
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);
  sp_str_t case_root = tmpfs_get(&harness->fs, sp_str_view(c.name));

  sp_str_t index_root = sp_fs_join_path(mem, case_root, sp_str_lit("index"));
  sp_fs_create_dir(index_root);

  write_publish_fixtures(harness, c.name, c.fixture.files);

  spn_index_info_t index = {
    .location = index_root,
    .protocol = SPN_INDEX_PROTOCOL_FILESYSTEM,
  };
  spn_index_init(&index, mem);

  spn_index_rel_t rel = {
    .id = {
      .namespace = sp_str_view(c.release.namespace),
      .name = sp_str_view(c.release.name),
    },
    .version = c.release.version,
  };
  if (c.release.source.url) { rel.source.url = sp_str_view(c.release.source.url); }
  if (c.release.source.rev) { rel.source.rev = sp_str_view(c.release.source.rev); }

  spn_err_t result = spn_index_publish(&index, &rel).kind;
  EXPECT_EQ(c.expect.result, result);

  if (c.expect.lines > 0) {
    sp_str_t ns = sp_str_view(c.release.namespace);
    sp_str_t nm = sp_str_view(c.release.name);
    sp_str_t pkg_path = sp_fs_join_path(mem,
      sp_fs_join_path(mem, index_root, ns),
      sp_fmt(mem, "{}.jsonl", sp_fmt_str(nm)).value
    );

    EXPECT_TRUE(sp_fs_exists(pkg_path));
    if (sp_fs_exists(pkg_path)) {
      sp_str_t content = test_read_file(mem, pkg_path);
      u32 count = 0;
      sp_str_for_line(content, line_it) {
        sp_str_t line = sp_str_trim(line_it.line);
        if (!sp_str_empty(line)) { count++; }
      }
      EXPECT_EQ(c.expect.lines, count);
    }
  }

  spn_index_deinit(&index);
}

UTEST_F(index_publish, publish_to_empty_index) {
  run_publish_case(utest_result, (publish_case_t) {
    .name = "publish_to_empty_index",
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .lines = 1,
    },
  });
}

UTEST_F(index_publish, publish_second_version) {
  run_publish_case(utest_result, (publish_case_t) {
    .name = "publish_second_version",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 1, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "def456" },
    },
    .expect = {
      .lines = 2,
    },
  });
}

UTEST_F(index_publish, publish_duplicate_version_rejected) {
  run_publish_case(utest_result, (publish_case_t) {
    .name = "publish_duplicate_version_rejected",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .result = SPN_ERR_VERSION_EXISTS,
      .lines = 1,
    },
  });
}

UTEST_F(index_publish, publish_creates_new_file_for_different_package) {
  run_publish_case(utest_result, (publish_case_t) {
    .name = "publish_creates_new_file_for_different_package",
    .fixture = {
      .files = {
        {
          .path = "core/other.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "other") ","
              kv("version", "1.0.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .lines = 1,
    },
  });
}
