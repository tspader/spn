#include "index.h"

typedef struct {
  const c8* path;
  const c8* lines [8];
} publish_file_t;

typedef struct {
  const c8* name;

  struct {
    publish_file_t files [4];
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
} publish_test_t;

static const publish_test_t tests [] = {
  {
    .name = "to_empty_index",
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = { 1, 0, 0 },
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .lines = 1,
    },
  },
  {
    .name = "second_version",
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
      .version = { 1, 1, 0 },
      .source = { .url = "https://github.com/example/spum", .rev = "def456" },
    },
    .expect = {
      .lines = 2,
    },
  },
  {
    .name = "duplicate_version_rejected",
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
      .version = { 1, 0, 0 },
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .result = SPN_ERR_VERSION_EXISTS,
      .lines = 1,
    },
  },
  {
    .name = "new_file_for_different_package",
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
      .version = { 1, 0, 0 },
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .lines = 1,
    },
  },
};

static void write_publish_fixtures(sp_mem_t mem, sp_str_t index_root, const publish_file_t files [4]) {
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

    sp_str_t path = sp_fs_join_path(mem, index_root, sp_str_view(file.path));
    sp_fs_create_dir(sp_fs_parent_path(path));
    sp_fs_create_file_str(path, sp_io_dyn_mem_writer_take_str(&builder));
  }
}

sp_test_each(index_publish, publish, publish_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t index_root = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index"));
  sp_fs_create_dir(index_root);

  write_publish_fixtures(mem, index_root, it->fixture.files);

  spn_index_info_t index = {
    .location = index_root,
    .protocol = SPN_INDEX_PROTOCOL_FILESYSTEM,
  };
  spn_index_init(&index, mem);

  spn_index_release_t rel = {
    .id = {
      .namespace = sp_str_view(it->release.namespace),
      .name = sp_str_view(it->release.name),
    },
    .version = it->release.version,
  };
  if (it->release.source.url) { rel.source.url = sp_str_view(it->release.source.url); }
  if (it->release.source.rev) { rel.source.rev = sp_str_view(it->release.source.rev); }

  spn_err_t result = spn_index_publish(&index, &rel).kind;
  sp_expect_eq(t, it->expect.result, result);

  if (it->expect.lines > 0) {
    sp_str_t ns = sp_str_view(it->release.namespace);
    sp_str_t nm = sp_str_view(it->release.name);
    sp_str_t pkg_path = sp_fs_join_path(mem,
      sp_fs_join_path(mem, index_root, ns),
      sp_fmt(mem, "{}.jsonl", sp_fmt_str(nm)).value
    );

    sp_must(t, sp_fs_exists(pkg_path));
    sp_expect_eq(t, it->expect.lines, index_test_count_lines(test_read_file(mem, pkg_path)));
  }

  spn_index_deinit(&index);
  return SP_OK;
}
