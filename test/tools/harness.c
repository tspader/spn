#include "test.h"
#include "utest.h"

#include "toml.h"
#include "stoml.h"

static void fixture_write_file(sp_str_t path, sp_str_t content) {
  sp_str_t parent = sp_fs_parent_path(path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&io, content);
  sp_io_writer_close(&io);
}

static sp_str_t fixture_registry_manifest_from_source(toml_table_t* source, sp_str_t repo_url) {
  toml_table_t* package = toml_table_table(source, "package");
  SP_ASSERT(package);

  spn_toml_writer_t writer = spn_toml_writer_new();
  spn_toml_begin_table_cstr(&writer, "package");
  spn_toml_append_str_cstr(&writer, "name", spn_toml_str(package, "name"));
  spn_toml_append_str_cstr(&writer, "version", spn_toml_str(package, "version"));
  spn_toml_append_str_cstr(&writer, "url", repo_url);

  sp_str_t author = spn_toml_str_opt(package, "author", "");
  if (!sp_str_empty(author)) {
    spn_toml_append_str_cstr(&writer, "author", author);
  }

  sp_str_t maintainer = spn_toml_str_opt(package, "maintainer", "");
  if (!sp_str_empty(maintainer)) {
    spn_toml_append_str_cstr(&writer, "maintainer", maintainer);
  }

  sp_str_t commit = spn_toml_str_opt(package, "commit", "");
  if (!sp_str_empty(commit)) {
    spn_toml_append_str_cstr(&writer, "commit", commit);
  }

  toml_array_t* include = toml_table_array(package, "include");
  if (spn_toml_array_len(include)) {
    spn_toml_append_str_array_cstr(&writer, "include", spn_toml_arr_to_str_arr(include));
  }

  toml_array_t* define = toml_table_array(package, "define");
  if (spn_toml_array_len(define)) {
    spn_toml_append_str_array_cstr(&writer, "define", spn_toml_arr_to_str_arr(define));
  }

  toml_array_t* system_deps = toml_table_array(package, "system_deps");
  if (spn_toml_array_len(system_deps)) {
    spn_toml_append_str_array_cstr(&writer, "system_deps", spn_toml_arr_to_str_arr(system_deps));
  }
  spn_toml_end_table(&writer);

  toml_table_t* lib = toml_table_table(source, "lib");
  if (lib) {
    toml_array_t* kinds = toml_table_array(lib, "kinds");
    if (spn_toml_array_len(kinds)) {
      spn_toml_begin_table_cstr(&writer, "lib");
      spn_toml_append_str_array_cstr(&writer, "kinds", spn_toml_arr_to_str_arr(kinds));
      spn_toml_end_table(&writer);
    }
  }

  return spn_toml_writer_write(&writer);
}

void copy_project_path(s32* result, tmpfs_t* fs, sp_str_t project, sp_str_t relative) {
  UTEST_RESULT(result);

  sp_str_t from = sp_fs_join_path(project, relative);

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

void setup_fixture_index_from_remote(s32* result, tmpfs_t* fs, sp_str_t index, sp_str_t project) {
  UTEST_RESULT(result);

  sp_str_t remote = sp_fs_join_path(project, sp_str_lit("remote"));
  if (!sp_fs_exists(remote)) {
    return;
  }

  ASSERT_TRUE(sp_fs_is_dir(remote));

  sp_da(sp_os_dir_ent_t) entries = sp_fs_collect(remote);
  sp_da_for(entries, it) {
    sp_os_dir_ent_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->file_path)) {
      continue;
    }

    sp_da(sp_os_dir_ent_t) versions = sp_fs_collect(entry->file_path);
    ASSERT_FALSE(sp_da_empty(versions));
    sp_dyn_array_sort(versions, dir_entry_sort_kernel_by_name);

    struct {
      sp_str_t repo;
      sp_str_t index;
      sp_str_t manifest;
      sp_str_t metadata;
    } paths = SP_ZERO_INITIALIZE();
    paths.repo = tmpfs_get(fs, sp_fs_join_path(sp_str_lit("remote"), entry->file_name));
    paths.index = sp_fs_join_path(index, entry->file_name);
    paths.manifest = sp_fs_join_path(paths.index, sp_str_lit("spn.toml"));
    paths.metadata = sp_fs_join_path(paths.index, sp_str_lit("metadata.toml"));

    git_repo_init(paths.repo);
    sp_fs_create_dir(paths.index);

    sp_os_dir_ent_t* latest = &versions[sp_da_size(versions) - 1];
    ASSERT_TRUE(sp_fs_is_dir(latest->file_path));

    struct {
      sp_str_t manifest;
      sp_str_t script;
    } latest_source = {
      .manifest = sp_fs_join_path(latest->file_path, sp_str_lit("spn.toml")),
      .script = sp_fs_join_path(latest->file_path, sp_str_lit("spn.c")),
    };

    ASSERT_TRUE(sp_fs_exists(latest_source.manifest));
    ASSERT_TRUE(sp_fs_exists(latest_source.script));

    toml_table_t* manifest = spn_toml_parse(latest_source.manifest);
    ASSERT_TRUE(manifest != SP_NULLPTR);

    toml_table_t* package = toml_table_table(manifest, "package");
    ASSERT_TRUE(package != SP_NULLPTR);

    fixture_write_file(paths.manifest, fixture_registry_manifest_from_source(manifest, paths.repo));
    sp_fs_copy(latest_source.script, paths.index);

    spn_toml_writer_t metadata = spn_toml_writer_new();
    spn_toml_begin_array_cstr(&metadata, "versions");

    sp_da_for(versions, v) {
      sp_os_dir_ent_t* dir = &versions[v];
      ASSERT_TRUE(sp_fs_is_dir(dir->file_path));

      sp_str_t source_manifest = sp_fs_join_path(dir->file_path, sp_str_lit("spn.toml"));
      ASSERT_TRUE(sp_fs_exists(source_manifest));

      git_repo_commit_from_dir(dir->file_path, paths.repo, dir->file_name);
      sp_str_t commit = git_repo_head(paths.repo);

      spn_toml_append_array_table(&metadata);
      spn_toml_append_str_cstr(&metadata, "version", dir->file_name);
      spn_toml_append_str_cstr(&metadata, "commit", commit);
    }

    spn_toml_end_array(&metadata);
    fixture_write_file(paths.metadata, spn_toml_writer_write(&metadata));
  }
}
