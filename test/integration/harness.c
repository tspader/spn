#include "sp.h"
#include "utest.h"
#include "test.h"

static void fixture_write_file(sp_str_t path, sp_str_t content) {
  sp_str_t parent = sp_fs_parent_path(path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_file_writer_t f = sp_zero;
  sp_io_file_writer_from_path(&f, path);
  sp_io_write_str(&f.base, content, SP_NULLPTR);
  sp_io_file_writer_close(&f);
}

void copy_project_path(s32* result, tmpfs_t* fs, sp_str_t project, sp_str_t relative) {
  UTEST_RESULT(result);

  sp_str_t from = sp_fs_join_path(spn_allocator, project, relative);

  // there is no reason to specify something that does not exist
  if (sp_fs_is_glob(from)) {
    ASSERT_TRUE(sp_fs_exists(sp_fs_parent_path(from)));
  } else {
    ASSERT_TRUE(sp_fs_exists(from));
  }

  sp_str_t to = fs->root;

  // we never want to change paths inside the test harness; always 1:1
  sp_str_t parent = sp_fs_parent_path(relative);
  if (!sp_str_empty(parent)) {
    to = tmpfs_get(fs, parent);
    sp_fs_create_dir(to);
  }

  // source exists, destination exists, copy
  sp_fs_copy(from, to);
}

s32 sort_dirs_by_name(const void* a, const void* b) {
  const sp_fs_entry_t* lhs = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* rhs = (const sp_fs_entry_t*)b;
  return sp_str_sort_kernel_alphabetical(&lhs->name, &rhs->name);
}

static sp_str_t build_release_json(sp_str_t name, sp_str_t version, sp_str_t repo_url, sp_str_t commit) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append_cstr(&b, "{\"namespace\":\"core\",\"name\":\"");
  sp_str_builder_append(&b, name);
  sp_str_builder_append_cstr(&b, "\",\"version\":\"");
  sp_str_builder_append(&b, version);
  sp_str_builder_append_cstr(&b, "\",\"yanked\":false,\"source\":{\"url\":\"");
  sp_str_builder_append(&b, sp_str_replace_c8(spn_allocator, repo_url, '\\', '/'));
  sp_str_builder_append_cstr(&b, "\",\"rev\":\"");
  sp_str_builder_append(&b, commit);
  sp_str_builder_append_cstr(&b, "\",\"dir\":\"\"},\"paths\":{\"manifest\":\"spn.toml\",\"script\":\"spn.c\"},\"deps\":[]}");
  return sp_str_builder_to_str(&b);
}

void setup_fixture_index_from_remote(s32* result, tmpfs_t* fs, sp_str_t index, sp_str_t project) {
  UTEST_RESULT(result);

  sp_str_t remote = sp_fs_join_path(spn_allocator, project, sp_str_lit("remote"));
  if (!sp_fs_exists(remote)) {
    return;
  }

  ASSERT_TRUE(sp_fs_is_dir(remote));

  // create core/ namespace directory under index
  sp_str_t core_dir = sp_fs_join_path(spn_allocator, index, sp_str_lit("core"));
  sp_fs_create_dir(core_dir);

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(spn_allocator, remote);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_da(sp_fs_entry_t) versions = sp_fs_collect(spn_allocator, entry->path);
    ASSERT_FALSE(sp_da_empty(versions));
    sp_dyn_array_sort(versions, sort_dirs_by_name);

    sp_str_t repo = tmpfs_get(fs, sp_fs_join_path(spn_allocator, sp_str_lit("remote"), entry->name));
    sp_str_t jsonl_path = sp_fs_join_path(spn_allocator, core_dir, sp_format("{}.jsonl", SP_FMT_STR(entry->name)));

    git_repo_init(repo);

    sp_str_builder_t jsonl = SP_ZERO_INITIALIZE();

    sp_da_for(versions, v) {
      sp_fs_entry_t* dir = &versions[v];
      ASSERT_TRUE(sp_fs_is_dir(dir->path));

      sp_str_t source_manifest = sp_fs_join_path(spn_allocator, dir->path, sp_str_lit("spn.toml"));
      ASSERT_TRUE(sp_fs_exists(source_manifest));

      git_repo_commit_from_dir(dir->path, repo, dir->name);
      sp_str_t commit = git_repo_head(repo);

      sp_str_t line = build_release_json(entry->name, dir->name, repo, commit);
      sp_str_builder_append(&jsonl, line);
      sp_str_builder_new_line(&jsonl);
    }

    fixture_write_file(jsonl_path, sp_str_builder_to_str(&jsonl));
  }
}

void setup_fixture_envrc(tmpfs_t* fs, sp_str_t storage, sp_str_t config) {
  sp_str_t path = tmpfs_get(fs, sp_str_lit(".envrc"));
  sp_str_t content = sp_format(
    "export SPN_STORAGE_DIR={}\n"
    "export SPN_CONFIG_DIR={}\n",
    SP_FMT_STR(storage),
    SP_FMT_STR(config)
  );
  fixture_write_file(path, content);
}

void setup_fixture_config(tmpfs_t* fs, sp_str_t config_dir, sp_str_t index_dir, sp_str_t spn_dir) {
  sp_str_t spn_config_dir = sp_fs_join_path(spn_allocator, config_dir, sp_str_lit("spn"));
  sp_fs_create_dir(spn_config_dir);

  sp_str_t config_path = sp_fs_join_path(spn_allocator, spn_config_dir, sp_str_lit("spn.toml"));
  sp_str_t content = sp_format(
    "spn = \"{}\"\n"
    "\n"
    "[[index]]\n"
    "name = \"core\"\n"
    "url = \"{}\"\n"
    "protocol = \"filesystem\"\n",
    SP_FMT_STR(sp_str_replace_c8(spn_allocator, spn_dir, '\\', '/')),
    SP_FMT_STR(sp_str_replace_c8(spn_allocator, index_dir, '\\', '/'))
  );
  fixture_write_file(config_path, content);
}
