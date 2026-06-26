#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "codegen/codegen.h"
#include "manifest.gen.h"

UTEST_MAIN();

static s32 manifest_gen_sort_entries(const void* a, const void* b) {
  const sp_fs_entry_t* ea = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* eb = (const sp_fs_entry_t*)b;
  return sp_str_sort_kernel_alphabetical(&ea->name, &eb->name);
}

static sp_mem_t manifest_gen_mem(void) {
  return sp_mem_heap_as_allocator(sp_mem_heap_new());
}

static spn_codegen_ctx_t manifest_gen_ctx(sp_mem_t mem) {
  spn_codegen_ctx_t ctx = sp_zero;
  spn_codegen_ctx_init(&ctx, mem, mem, sp_intern_new(mem));
  return ctx;
}

static sp_str_t manifest_gen_render(sp_mem_t mem, sp_fs_entry_t* entry) {
  spn_codegen_ctx_t ctx = manifest_gen_ctx(mem);

  spn_cg_root_t manifest = sp_zero;
  if (spn_codegen_load(&ctx, entry->path, &manifest)) {
    return spn_codegen_issues_to_str(mem, ctx.issues);
  }

  return spn_cg_root_write(mem, &manifest);
}

UTEST(manifest_gen, missing_file) {
  sp_mem_t mem = manifest_gen_mem();
  spn_codegen_ctx_t ctx = manifest_gen_ctx(mem);

  spn_cg_root_t manifest = sp_zero;
  bool failed = spn_codegen_load(&ctx, sp_str_lit("/nonexistent/missing.toml"), &manifest);

  ASSERT_TRUE(failed);
  ASSERT_EQ((u32)1, (u32)sp_da_size(ctx.issues));
  EXPECT_EQ(SPN_CODEGEN_ERR_FILE_MISSING, ctx.issues[0].code);
  EXPECT_TRUE(sp_str_equal(ctx.issues[0].detail, sp_str_lit("missing file")));
}

UTEST(manifest_gen, corpus) {
  sp_mem_t mem = manifest_gen_mem();

  struct {
    sp_str_t toml;
    sp_str_t json;
  } paths = {
    .toml = sp_cstr_as_str(MANIFEST_DIR),
    .json = sp_cstr_as_str(GOLDEN_DIR)
  };

  sp_env_t env = sp_env_capture(mem);
  bool regen = sp_env_contains_c(&env, "SPN_GOLDEN_REGEN");

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, paths.toml);
  sp_da_sort(entries, manifest_gen_sort_entries);

  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];

    struct {
      sp_str_t name;
      sp_str_t file;
      sp_str_t path;
    } golden = sp_zero;
    golden.name = sp_fs_get_stem(entry->name);
    golden.file = sp_fmt(mem, "{}.json", sp_fmt_str(golden.name)).value;
    golden.path = sp_fs_join_path(mem, paths.json, golden.file);

    sp_str_t expected = manifest_gen_render(mem, entry);

    if (regen) {
      sp_fs_create_file_str(golden.path, expected);
      continue;
    }

    sp_str_t actual = sp_zero;
    sp_io_read_file(mem, golden.path, &actual);
    EXPECT_TRUE(sp_str_equal(actual, expected));
  }
}
