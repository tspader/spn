#include "mz.h"
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

UTEST_MAIN();

#define ut (*utest_fixture)
#define ur (*utest_result)


typedef void(*mz_out_free_fn_t)(void* out, sp_allocator_t allocator);

typedef enum {
  MZ_TEST_NONE = 0,
  MZ_TEST_BOOL,
  MZ_TEST_S32,
  MZ_TEST_U64,
  MZ_TEST_F64,
  MZ_TEST_ARR_LEN,
  MZ_TEST_ARR_ENTRY_STR,
  MZ_TEST_ARR_ENTRY_S32,
  MZ_TEST_ARR_ENTRY_CHILD_STR,
  MZ_TEST_ARR_ENTRY_CHILD_BOOL,
} mz_expect_kind_t;

typedef struct {
  u32 offset;
  u32 index;
  u32 field_offset;
  u32 item_size;
  mz_expect_kind_t kind;
  union {
    bool as_bool;
    s32 as_s32;
    u64 as_u64;
    f64 as_f64;
    u32 as_u32;
    const c8* as_cstr;
  } expected;
  f64 epsilon;
} mz_expect_value_t;

typedef enum {
  MZ_TEST_PTR_NONE = 0,
  MZ_TEST_PTR_CSTR,
  MZ_TEST_PTR_BOOL,
  MZ_TEST_PTR_S32,
  MZ_TEST_PTR_U64,
  MZ_TEST_PTR_F64,
  MZ_TEST_PTR_CHILD_STR,
  MZ_TEST_PTR_GRANDCHILD_STR,
} mz_expect_ptr_kind_t;

typedef struct {
  mz_expect_ptr_kind_t kind;
  u32 offset;
  u32 field_offset;
  u32 child_field_offset;
  union {
    const c8* as_cstr;
    bool as_bool;
    s32 as_s32;
    u64 as_u64;
    f64 as_f64;
  } expected;
  f64 epsilon;
} mz_expect_ptr_t;

#define MZ_EXPECT_PTR_MAX 8
#define MZ_EXPECT_VALUE_MAX 8
#define MZ_CASE_MAX 24

typedef struct {
  const c8* json;
  mz_err_t expected;
  const c8* expected_path;
} mz_validate_case_t;

typedef struct {
  const c8* json;
  mz_err_t expected;
  const c8* expected_path;
  mz_expect_value_t values[MZ_EXPECT_VALUE_MAX];
  mz_expect_ptr_t ptrs[MZ_EXPECT_PTR_MAX];
} mz_parse_case_t;

#define MZ_EXPECT_BOOL_FIELD(TYPE, FIELD, VALUE) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, FIELD), .kind = MZ_TEST_BOOL, .expected = { .as_bool = (VALUE) } })

#define MZ_EXPECT_S32_FIELD(TYPE, FIELD, VALUE) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, FIELD), .kind = MZ_TEST_S32, .expected = { .as_s32 = (VALUE) } })

#define MZ_EXPECT_U64_FIELD(TYPE, FIELD, VALUE) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, FIELD), .kind = MZ_TEST_U64, .expected = { .as_u64 = (VALUE) } })

#define MZ_EXPECT_F64(TYPE, FIELD, VALUE, EPS) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, FIELD), .kind = MZ_TEST_F64, .expected = { .as_f64 = (VALUE) }, .epsilon = (EPS) })

#define MZ_EXPECT_ARR_LEN(TYPE, FIELD, LEN) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, FIELD), .kind = MZ_TEST_ARR_LEN, .expected = { .as_u32 = (LEN) } })

#define MZ_EXPECT_ARR_ENTRY_STR(TYPE, FIELD, INDEX, CSTR) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, FIELD), .index = (INDEX), .kind = MZ_TEST_ARR_ENTRY_STR, .expected = { .as_cstr = (CSTR) } })

#define MZ_EXPECT_ARR_ENTRY_S32(TYPE, FIELD, INDEX, VALUE) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, FIELD), .index = (INDEX), .kind = MZ_TEST_ARR_ENTRY_S32, .expected = { .as_s32 = (VALUE) } })

#define MZ_EXPECT_ARR_ENTRY_CHILD_STR(TYPE, DA_FIELD, INDEX, ITEM_TYPE, ITEM_FIELD, CSTR) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, DA_FIELD), .index = (INDEX), .field_offset = offsetof(ITEM_TYPE, ITEM_FIELD), .item_size = sizeof(ITEM_TYPE), .kind = MZ_TEST_ARR_ENTRY_CHILD_STR, .expected = { .as_cstr = (CSTR) } })

#define MZ_EXPECT_ARR_ENTRY_CHILD_BOOL(TYPE, DA_FIELD, INDEX, ITEM_TYPE, ITEM_FIELD, BOOL_VALUE) \
  ((mz_expect_value_t) { .offset = offsetof(TYPE, DA_FIELD), .index = (INDEX), .field_offset = offsetof(ITEM_TYPE, ITEM_FIELD), .item_size = sizeof(ITEM_TYPE), .kind = MZ_TEST_ARR_ENTRY_CHILD_BOOL, .expected = { .as_bool = (BOOL_VALUE) } })

#define MZ_EXPECT_PTR_STR(TYPE, FIELD, LIT) \
  ((mz_expect_ptr_t) { .kind = MZ_TEST_PTR_CSTR, .offset = offsetof(TYPE, FIELD), .expected = { .as_cstr = (LIT) } })

#define MZ_EXPECT_CHILD_STR(TYPE, PTR_FIELD, ITEM_TYPE, ITEM_FIELD, LIT) \
  ((mz_expect_ptr_t) { .kind = MZ_TEST_PTR_CHILD_STR, .offset = offsetof(TYPE, PTR_FIELD), .field_offset = offsetof(ITEM_TYPE, ITEM_FIELD), .expected = { .as_cstr = (LIT) } })

#define MZ_EXPECT_GRANDCHILD_STR(TYPE, PTR_FIELD, ITEM_TYPE, CHILD_PTR_FIELD, CHILD_TYPE, CHILD_FIELD, LIT) \
  ((mz_expect_ptr_t) { .kind = MZ_TEST_PTR_GRANDCHILD_STR, .offset = offsetof(TYPE, PTR_FIELD), .field_offset = offsetof(ITEM_TYPE, CHILD_PTR_FIELD), .child_field_offset = offsetof(CHILD_TYPE, CHILD_FIELD), .expected = { .as_cstr = (LIT) } })

#define MZ_EXPECT_PTR_BOOL(TYPE, FIELD, VALUE) \
  ((mz_expect_ptr_t) { .kind = MZ_TEST_PTR_BOOL, .offset = offsetof(TYPE, FIELD), .expected = { .as_bool = (VALUE) } })

#define MZ_EXPECT_PTR_S32(TYPE, FIELD, VALUE) \
  ((mz_expect_ptr_t) { .kind = MZ_TEST_PTR_S32, .offset = offsetof(TYPE, FIELD), .expected = { .as_s32 = (VALUE) } })

#define MZ_EXPECT_PTR_U64(TYPE, FIELD, VALUE) \
  ((mz_expect_ptr_t) { .kind = MZ_TEST_PTR_U64, .offset = offsetof(TYPE, FIELD), .expected = { .as_u64 = (VALUE) } })

#define MZ_EXPECT_PTR_F64(TYPE, FIELD, VALUE, EPS) \
  ((mz_expect_ptr_t) { .kind = MZ_TEST_PTR_F64, .offset = offsetof(TYPE, FIELD), .expected = { .as_f64 = (VALUE) }, .epsilon = (EPS) })


static void assert_value_expectations(s32* utest_result, const void* out, const mz_expect_value_t values[MZ_EXPECT_VALUE_MAX]) {
  SP_UNUSED(utest_result);

  sp_for(it, MZ_EXPECT_VALUE_MAX) {
    const mz_expect_value_t* exp = &values[it];
    if (exp->kind == MZ_TEST_NONE) {
      break;
    }

    switch (exp->kind) {
      case MZ_TEST_BOOL: {
        const u8* base = (const u8*)out + exp->offset;
        EXPECT_EQ(exp->expected.as_bool ? 1 : 0, *((const bool*)base) ? 1 : 0);
        break;
      }
      case MZ_TEST_S32: {
        const u8* base = (const u8*)out + exp->offset;
        EXPECT_EQ(exp->expected.as_s32, *((const s32*)base));
        break;
      }
      case MZ_TEST_U64: {
        const u8* base = (const u8*)out + exp->offset;
        EXPECT_EQ(exp->expected.as_u64, *((const u64*)base));
        break;
      }
      case MZ_TEST_F64: {
        const u8* base = (const u8*)out + exp->offset;
        f64 got = *((const f64*)base);
        f64 lo = exp->expected.as_f64 - exp->epsilon;
        f64 hi = exp->expected.as_f64 + exp->epsilon;
        EXPECT_TRUE(got >= lo && got <= hi);
        break;
      }
      case MZ_TEST_ARR_LEN: {
        const void* da = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_EQ(exp->expected.as_u32, sp_da_size(da));
        break;
      }
      case MZ_TEST_ARR_ENTRY_STR: {
        const c8** da = *((const c8** const*)((const u8*)out + exp->offset));
        EXPECT_TRUE(sp_cstr_equal(exp->expected.as_cstr, da[exp->index]));
        break;
      }
      case MZ_TEST_ARR_ENTRY_S32: {
        const s32* da = *((const s32* const*)((const u8*)out + exp->offset));
        EXPECT_EQ(exp->expected.as_s32, da[exp->index]);
        break;
      }
      case MZ_TEST_ARR_ENTRY_CHILD_STR: {
        const void* da = *((void* const*)((const u8*)out + exp->offset));
        const u8* item_base = (const u8*)da + (exp->index * exp->item_size);
        const c8* item_cstr = *((const c8* const*)(item_base + exp->field_offset));
        EXPECT_TRUE(sp_cstr_equal(exp->expected.as_cstr, item_cstr));
        break;
      }
      case MZ_TEST_ARR_ENTRY_CHILD_BOOL: {
        const void* da = *((void* const*)((const u8*)out + exp->offset));
        const u8* item_base = (const u8*)da + (exp->index * exp->item_size);
        bool item_bool = *((const bool*)(item_base + exp->field_offset));
        EXPECT_EQ(exp->expected.as_bool ? 1 : 0, item_bool ? 1 : 0);
        break;
      }
      case MZ_TEST_NONE: {
        break;
      }
    }
  }
}

static void assert_ptr_expectations(s32* utest_result, const void* out, const mz_expect_ptr_t ptrs[MZ_EXPECT_PTR_MAX]) {
  SP_UNUSED(utest_result);

  sp_for(it, MZ_EXPECT_PTR_MAX) {
    const mz_expect_ptr_t* exp = &ptrs[it];
    if (exp->kind == MZ_TEST_PTR_NONE) {
      break;
    }

    switch (exp->kind) {
      case MZ_TEST_PTR_CSTR: {
        const void* ptr = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)ptr);
        EXPECT_TRUE(sp_cstr_equal(exp->expected.as_cstr, (const c8*)ptr));
        break;
      }
      case MZ_TEST_PTR_CHILD_STR: {
        const void* ptr = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)ptr);
        const c8* value = *((const c8* const*)((const u8*)ptr + exp->field_offset));
        EXPECT_TRUE(sp_cstr_equal(exp->expected.as_cstr, value));
        break;
      }
      case MZ_TEST_PTR_GRANDCHILD_STR: {
        const void* ptr = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)ptr);
        const void* child_ptr = *((void* const*)((const u8*)ptr + exp->field_offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)child_ptr);
        const c8* value = *((const c8* const*)((const u8*)child_ptr + exp->child_field_offset));
        EXPECT_TRUE(sp_cstr_equal(exp->expected.as_cstr, value));
        break;
      }
      case MZ_TEST_PTR_BOOL: {
        const void* ptr = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)ptr);
        EXPECT_EQ(exp->expected.as_bool ? 1 : 0, *((const bool*)ptr) ? 1 : 0);
        break;
      }
      case MZ_TEST_PTR_S32: {
        const void* ptr = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)ptr);
        EXPECT_EQ(exp->expected.as_s32, *((const s32*)ptr));
        break;
      }
      case MZ_TEST_PTR_U64: {
        const void* ptr = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)ptr);
        EXPECT_EQ(exp->expected.as_u64, *((const u64*)ptr));
        break;
      }
      case MZ_TEST_PTR_F64: {
        const void* ptr = *((void* const*)((const u8*)out + exp->offset));
        EXPECT_NE((void*)SP_NULLPTR, (void*)ptr);
        f64 got = *((const f64*)ptr);
        f64 lo = exp->expected.as_f64 - exp->epsilon;
        f64 hi = exp->expected.as_f64 + exp->epsilon;
        EXPECT_TRUE(got >= lo && got <= hi);
        break;
      }
      case MZ_TEST_PTR_NONE: {
        break;
      }
    }
  }
}

static void run_validate_cases(s32* utest_result, mz_schema_t* schema, const mz_validate_case_t cases[MZ_CASE_MAX]) {
  SP_UNUSED(utest_result);
  EXPECT_NE((void*)SP_NULLPTR, (void*)schema);

  sp_for(it, MZ_CASE_MAX) {
    if (!cases[it].json) {
      break;
    }

    mz_ctx_t ctx = SP_ZERO_INITIALIZE();
    mz_parse_ctx_init(&ctx);

    mz_err_t err = mz_validate_str_ex(schema, cases[it].json, &ctx);
    EXPECT_EQ((s32)cases[it].expected, (s32)err);
    if (err != MZ_OK) {
      EXPECT_EQ((s32)err, (s32)ctx.diag.kind);
    }

    if (cases[it].expected_path) {
      EXPECT_TRUE(sp_cstr_equal(cases[it].expected_path, ctx.diag.path));
    }
    mz_parse_ctx_destroy(&ctx);
  }
}

static void run_parse_cases(s32* utest_result, mz_schema_t* schema, void* result, const mz_parse_case_t cases[MZ_CASE_MAX]) {
  SP_UNUSED(utest_result);
  EXPECT_NE((void*)SP_NULLPTR, (void*)schema);

  sp_for(it, MZ_CASE_MAX) {
    if (!cases[it].json) {
      break;
    }

    mz_ctx_t ctx = SP_ZERO_INITIALIZE();
    mz_parse_ctx_init(&ctx);

    mz_err_t err = mz_parse_str_ex(schema, cases[it].json, result, &ctx);
    EXPECT_EQ((s32)cases[it].expected, (s32)err);
    if (err != MZ_OK) {
      EXPECT_EQ((s32)err, (s32)ctx.diag.kind);
    }

    if (cases[it].expected_path) {
      EXPECT_TRUE(sp_cstr_equal(cases[it].expected_path, ctx.diag.path));
    }

    if (err == MZ_OK) {
      assert_value_expectations(utest_result, result, cases[it].values);
      assert_ptr_expectations(utest_result, result, cases[it].ptrs);
    }
    mz_parse_ctx_destroy(&ctx);
  }
}

typedef struct leaf_t child_t;

struct leaf_t {
  const c8* key;
  const c8* str;
  bool boolean;
  s32 s32;
  u64 u64;
  f64 f64;
  child_t* ptr;
  child_t* items;
};

typedef struct {
  const c8* str;
  bool boolean;
  s32 s32;
  u64 u64;
  f64 f64;
  s32 sentinel;
  child_t leaf;
  s32* s32_ptr;
  const c8** str_items;
  s32* s32_items;
  child_t* child;
} root_t;

typedef struct {
  const c8* type;
  const c8* str;
  s32 s32;
} tagged_str_t;

typedef struct {
  const c8* str;
  bool boolean;
  s32 s32;
  u64 u64;
} tagged_int_t;

static mz_err_t on_insert_scalar_ok(mz_ctx_t* ctx, void* out, const c8* key, void** value_out) {
  SP_UNUSED(ctx);
  child_t* items = *((child_t**)out);
  sp_da_push(items, ((child_t) { .key = sp_cstr_copy(key) }));
  *((child_t**)out) = items;
  *value_out = &items[sp_da_size(items) - 1].str;
  return MZ_OK;
}

static mz_err_t on_insert_object_ok(mz_ctx_t* ctx, void* out, const c8* key, void** value_out) {
  SP_UNUSED(ctx);
  child_t* items = *((child_t**)out);
  sp_da_push(items, ((child_t) { .key = sp_cstr_copy(key) }));
  *((child_t**)out) = items;
  *value_out = &items[sp_da_size(items) - 1];
  return MZ_OK;
}

static mz_err_t on_insert_fail(mz_ctx_t* ctx, void* out, const c8* key, void** value_out) {
  SP_UNUSED(ctx);
  SP_UNUSED(out);
  SP_UNUSED(key);
  SP_UNUSED(value_out);
  return MZ_ERR_RANGE;
}

static mz_err_t on_insert_null_out(mz_ctx_t* ctx, void* out, const c8* key, void** value_out) {
  SP_UNUSED(ctx);
  SP_UNUSED(out);
  SP_UNUSED(key);
  *value_out = SP_NULLPTR;
  return MZ_OK;
}

static mz_err_t on_resolve_s32_ok(mz_ctx_t* ctx, void* parent_out, const c8* key, void** value_out) {
  SP_UNUSED(parent_out);
  SP_UNUSED(key);
  sp_context_push_allocator(ctx->allocator);
  *value_out = sp_alloc_type(s32);
  sp_context_pop();
  return MZ_OK;
}

static mz_err_t on_resolve_s32_fail(mz_ctx_t* ctx, void* parent_out, const c8* key, void** value_out) {
  SP_UNUSED(ctx);
  SP_UNUSED(parent_out);
  SP_UNUSED(key);
  SP_UNUSED(value_out);
  return MZ_ERR_RANGE;
}

static mz_err_t on_resolve_s32_null(mz_ctx_t* ctx, void* parent_out, const c8* key, void** value_out) {
  SP_UNUSED(ctx);
  SP_UNUSED(parent_out);
  SP_UNUSED(key);
  *value_out = SP_NULLPTR;
  return MZ_OK;
}

static mz_err_t on_resolve_leaf_ok(mz_ctx_t* ctx, void* parent_out, const c8* key, void** value_out) {
  SP_UNUSED(parent_out);
  SP_UNUSED(key);
  sp_context_push_allocator(ctx->allocator);
  *value_out = sp_alloc_type(child_t);
  sp_context_pop();
  return MZ_OK;
}

struct mz {
  mz_builder_t* b;
  sp_mem_arena_t* arena;
  sp_mem_arena_marker_t mark;
};

#define q(str) #str
#define k(key) q(key) ": "
#define kv(key, val) q(key) ": " #val

UTEST_F_SETUP(mz) {
  ut.arena = sp_mem_arena_new(512);
  ut.mark = sp_mem_arena_mark(ut.arena);
  sp_context_push_allocator(sp_mem_arena_as_allocator(ut.arena));

  ut.b = sp_alloc_type(mz_builder_t);
  *ut.b = mz_builder_begin();
}

UTEST_F_TEARDOWN(mz) {
  sp_context_pop();
  sp_mem_arena_pop(ut.mark);
  sp_mem_arena_destroy(ut.arena);
}

UTEST_F(mz, core_object_strict_semantics) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, str, "str", mz_schema_string());
    MZ_OPT_BIND(ut.b, root_t, boolean, "boolean", mz_schema_bool());
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("str", "ok")
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_BOOL_FIELD(root_t, boolean, false),
      },
      .ptrs = {
        MZ_EXPECT_PTR_STR(root_t, str, "ok"),
      },
    },
    {
      .json = "{"
        kv("str", "ok") ","
        kv("extra", 1)
      "}",
      .expected = MZ_ERR_UNKNOWN_KEY,
      .expected_path = "root.extra",
    },
    {
      .json = "{"
        "}",
      .expected = MZ_ERR_MISSING_KEY,
      .expected_path = "root.str",
    },
    {
      .json = "{"
        kv("str", "ok") ","
        kv("boolean", true)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_BOOL_FIELD(root_t, boolean, true),
      },
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, core_object_loose_semantics) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_LOOSE) {
    MZ_BIND(ut.b, root_t, str, "str", mz_schema_string());
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("str", "ok") ","
        kv("extra", 1)
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_STR(root_t, str, "ok"),
      },
    },
    {
      .json = "{"
        kv("str", "ok") ","
        kv("extra", 1) ","
        k("nested") "{"
          kv("str", "ok")
        "}"
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_STR(root_t, str, "ok"),
      },
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, nested_parent_strict_child_loose) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, str, "str", mz_schema_string());
    MZ_OBJECT_BIND(ut.b, root_t, leaf, "leaf", MZ_OBJECT_LOOSE) {
      MZ_BIND(ut.b, child_t, str, "str", mz_schema_string());
    }
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("str", "r") ","
        k("leaf") "{"
          kv("str", "l") ","
          kv("extra", 1)
        "}"
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_STR(root_t, str, "r"),
        MZ_EXPECT_PTR_STR(root_t, leaf.str, "l"),
      },
    },
    {
      .json = "{"
        kv("str", "r") ","
        k("leaf") "{"
          kv("str", "l")
        "},"
        kv("extra", 1)
      "}",
      .expected = MZ_ERR_UNKNOWN_KEY,
      .expected_path = "root.extra",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, nested_parent_loose_child_strict) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_LOOSE) {
    MZ_BIND(ut.b, root_t, str, "str", mz_schema_string());
    MZ_OBJECT_BIND(ut.b, root_t, leaf, "leaf", MZ_OBJECT_STRICT) {
      MZ_BIND(ut.b, child_t, str, "str", mz_schema_string());
    }
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("str", "r") ","
        k("leaf") "{"
          kv("str", "l")
        "},"
        kv("extra", 1)
      "}",
      .expected = MZ_OK,
    },
    {
      .json = "{"
        kv("str", "r") ","
        k("leaf") "{"
          kv("str", "l") ","
          kv("extra", 1)
        "}"
      "}",
      .expected = MZ_ERR_UNKNOWN_KEY,
      .expected_path = "root.leaf.extra",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, scalar_string) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, str, "str", mz_schema_string());
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("str", "s")
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_STR(root_t, str, "s"),
      },
    },
    {
      .json = "{"
        kv("str", 1)
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.str",
    },
    {
      .json = "{"
        kv("str", true)
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.str",
    },
    {
      .json = "{"
        k("str") "{"
        "}"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.str",
    },
    {
      .json = "{"
        k("str") "[]"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.str",
    },
    {
      .json = "{"
        kv("str", null)
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.str",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, scalar_bool) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, boolean, "boolean", mz_schema_bool());
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("boolean", true)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_BOOL_FIELD(root_t, boolean, true),
      },
    },
    {
      .json = "{"
        kv("boolean", false)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_BOOL_FIELD(root_t, boolean, false),
      },
    },
    {
      .json = "{"
        kv("boolean", 1)
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.boolean",
    },
    {
      .json = "{"
        kv("boolean", "x")
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.boolean",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, scalar_s32) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, s32, "s32", mz_schema_s32_ex(-2, 2));
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("s32", -2)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(root_t, s32, -2),
      },
    },
    {
      .json = "{"
        kv("s32", 0)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(root_t, s32, 0),
      },
    },
    {
      .json = "{"
        kv("s32", 2)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(root_t, s32, 2),
      },
    },
    {
      .json = "{"
        kv("s32", -3)
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.s32",
    },
    {
      .json = "{"
        kv("s32", 3)
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.s32",
    },
    {
      .json = "{"
        kv("s32", "0")
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.s32",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, scalar_u64) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, u64, "u64", mz_schema_u64_ex(0, 10));
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("u64", 0)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_U64_FIELD(root_t, u64, 0),
      },
    },
    {
      .json = "{"
        kv("u64", 5)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_U64_FIELD(root_t, u64, 5),
      },
    },
    {
      .json = "{"
        kv("u64", 10)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_U64_FIELD(root_t, u64, 10),
      },
    },
    {
      .json = "{"
        kv("u64", -1)
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.u64",
    },
    {
      .json = "{"
        kv("u64", 11)
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.u64",
    },
    {
      .json = "{"
        kv("u64", "1")
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.u64",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, scalar_f64) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, f64, "f64", mz_schema_f64_ex(-1.0, 1.0));
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("f64", 0)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_F64(root_t, f64, 0.0, 0.001),
      },
    },
    {
      .json = "{"
        kv("f64", 0.5)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_F64(root_t, f64, 0.5, 0.001),
      },
    },
    {
      .json = "{"
        kv("f64", -2.0)
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.f64",
    },
    {
      .json = "{"
        kv("f64", 2.0)
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.f64",
    },
    {
      .json = "{"
        kv("f64", "1.0")
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.f64",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, schema_any) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, s32, "s32", mz_schema_s32());
    MZ(ut.b, "payload", mz_schema_any());
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("s32", 7) ","
        k("payload") "{"
          kv("a", 1) ","
          kv("b", true) ","
          kv("c", null)
        "}"
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(root_t, s32, 7),
      },
    },
    {
      .json = "{"
        kv("s32", 8) ","
        k("payload") "[1,2,{" kv("x", "y") "}]"
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(root_t, s32, 8),
      },
    },
    {
      .json = "{"
        kv("s32", 9) ","
        kv("payload", "str")
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(root_t, s32, 9),
      },
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, tagged_union_str_auto_tag_field) {
  mz_schema_t* tagged = mz_schema_tagged_union("type", MZ_TAG_KIND_STR);

  mz_schema_t* alpha = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(alpha, "str", mz_schema_string(), true, offsetof(tagged_str_t, str));
  mz_tagged_union_add(tagged, mz_tag_str("alpha"), alpha);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("type", "alpha") ","
        kv("str", "hello")
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_STR(tagged_str_t, str, "hello"),
      },
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_str_t), cases);
}

UTEST_F(mz, tagged_union_str_explicit_bound_tag_field) {
  mz_schema_t* tagged = mz_schema_tagged_union("type", MZ_TAG_KIND_STR);

  mz_schema_t* beta = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(beta, "type", mz_schema_string(), true, offsetof(tagged_str_t, type));
  mz_object_add_field(beta, "s32", mz_schema_s32(), true, offsetof(tagged_str_t, s32));
  mz_tagged_union_add(tagged, mz_tag_str("beta"), beta);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("type", "beta") ","
        kv("s32", 7)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(tagged_str_t, s32, 7),
      },
      .ptrs = {
        MZ_EXPECT_PTR_STR(tagged_str_t, type, "beta"),
      },
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_str_t), cases);
}

UTEST_F(mz, tagged_union_str_strict_unknown_key) {
  mz_schema_t* tagged = mz_schema_tagged_union("type", MZ_TAG_KIND_STR);

  mz_schema_t* alpha = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(alpha, "str", mz_schema_string(), true, offsetof(tagged_str_t, str));
  mz_tagged_union_add(tagged, mz_tag_str("alpha"), alpha);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("type", "alpha") ","
        kv("str", "hello") ","
        kv("extra", 1)
      "}",
      .expected = MZ_ERR_UNKNOWN_KEY,
      .expected_path = "root.extra",
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_str_t), cases);
}

UTEST_F(mz, tagged_union_s32_auto_tag_field) {
  mz_schema_t* tagged = mz_schema_tagged_union("tag", MZ_TAG_KIND_S32);

  mz_schema_t* neg = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(neg, "str", mz_schema_string(), true, offsetof(tagged_int_t, str));
  mz_tagged_union_add(tagged, mz_tag_s32(-7), neg);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("tag", -7) ","
        kv("str", "neg")
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_STR(tagged_int_t, str, "neg"),
      },
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_int_t), cases);
}

UTEST_F(mz, tagged_union_s32_explicit_bound_tag_field) {
  mz_schema_t* tagged = mz_schema_tagged_union("tag", MZ_TAG_KIND_S32);

  mz_schema_t* pos = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(pos, "tag", mz_schema_s32(), true, offsetof(tagged_int_t, s32));
  mz_object_add_field(pos, "boolean", mz_schema_bool(), true, offsetof(tagged_int_t, boolean));
  mz_tagged_union_add(tagged, mz_tag_s32(8), pos);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("tag", 8) ","
        kv("boolean", true)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_S32_FIELD(tagged_int_t, s32, 8),
        MZ_EXPECT_BOOL_FIELD(tagged_int_t, boolean, true),
      },
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_int_t), cases);
}

UTEST_F(mz, tagged_union_s32_wrong_tag_type) {
  mz_schema_t* tagged = mz_schema_tagged_union("tag", MZ_TAG_KIND_S32);

  mz_schema_t* pos = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(pos, "boolean", mz_schema_bool(), true, offsetof(tagged_int_t, boolean));
  mz_tagged_union_add(tagged, mz_tag_s32(8), pos);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("tag", "8") ","
        kv("boolean", true)
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.tag",
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_int_t), cases);
}

UTEST_F(mz, tagged_union_u64_auto_tag_field) {
  mz_schema_t* tagged = mz_schema_tagged_union("tag", MZ_TAG_KIND_U64);

  mz_schema_t* low = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(low, "str", mz_schema_string(), true, offsetof(tagged_int_t, str));
  mz_tagged_union_add(tagged, mz_tag_u64(22), low);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("tag", 22) ","
        kv("str", "twenty-two")
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_STR(tagged_int_t, str, "twenty-two"),
      },
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_int_t), cases);
}

UTEST_F(mz, tagged_union_u64_explicit_bound_tag_field) {
  mz_schema_t* tagged = mz_schema_tagged_union("tag", MZ_TAG_KIND_U64);

  mz_schema_t* high = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(high, "tag", mz_schema_u64(), true, offsetof(tagged_int_t, u64));
  mz_object_add_field(high, "boolean", mz_schema_bool(), true, offsetof(tagged_int_t, boolean));
  mz_tagged_union_add(tagged, mz_tag_u64(99), high);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("tag", 99) ","
        kv("boolean", true)
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_U64_FIELD(tagged_int_t, u64, 99),
        MZ_EXPECT_BOOL_FIELD(tagged_int_t, boolean, true),
      },
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_int_t), cases);
}

UTEST_F(mz, tagged_union_u64_negative_tag) {
  mz_schema_t* tagged = mz_schema_tagged_union("tag", MZ_TAG_KIND_U64);

  mz_schema_t* high = mz_schema_object(MZ_OBJECT_STRICT);
  mz_object_add_field(high, "boolean", mz_schema_bool(), true, offsetof(tagged_int_t, boolean));
  mz_tagged_union_add(tagged, mz_tag_u64(99), high);

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("tag", -1) ","
        kv("boolean", true)
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.tag",
    },
  };

  run_parse_cases(&ur, tagged, sp_alloc_type(tagged_int_t), cases);
}

UTEST_F(mz, arrays) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, str_items, "str_items", mz_schema_array_ex(mz_schema_string(), 1, 2));
    MZ_BIND(ut.b, root_t, s32_items, "s32_items", mz_schema_array_ex(mz_schema_s32_ex(0, 10), 1, 2));
    MZ_ARRAY_BIND(ut.b, root_t, child, "child", MZ_OBJECT_STRICT) {
      MZ_BIND(ut.b, child_t, str, "str", mz_schema_string());
    }
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        k("str_items") "[" q("a") "," q("b") "],"
        k("s32_items") "[1,2],"
        k("child") "["
          "{"
            kv("str", "x")
          "}"
        "]"
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_ARR_LEN(root_t, str_items, 2),
        MZ_EXPECT_ARR_ENTRY_STR(root_t, str_items, 1, "b"),
        MZ_EXPECT_ARR_LEN(root_t, s32_items, 2),
        MZ_EXPECT_ARR_ENTRY_S32(root_t, s32_items, 1, 2),
        MZ_EXPECT_ARR_LEN(root_t, child, 1),
        MZ_EXPECT_ARR_ENTRY_CHILD_STR(root_t, child, 0, child_t, str, "x"),
      },
    },
    {
      .json = "{"
        k("str_items") "{},"
        k("s32_items") "[1],"
        k("child") "[]"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.str_items",
    },
    {
      .json = "{"
        k("str_items") "[],"
        k("s32_items") "[1],"
        k("child") "[]"
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.str_items",
    },
    {
      .json = "{"
        k("str_items") "[" q("a") "," q("b") "," q("c") "],"
        k("s32_items") "[1],"
        k("child") "[]"
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.str_items",
    },
    {
      .json = "{"
        k("str_items") "[1],"
        k("s32_items") "[1],"
        k("child") "[]"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.str_items[0]",
    },
    {
      .json = "{"
        k("str_items") "[" q("a") "],"
        k("s32_items") "[11],"
        k("child") "[]"
      "}",
      .expected = MZ_ERR_RANGE,
      .expected_path = "root.s32_items[0]",
    },
    {
      .json = "{"
        k("str_items") "[" q("a") "],"
        k("s32_items") "[1],"
        k("child") "["
          "{"
            kv("str", 1)
          "}"
        "]"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.child[0].str",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, map_scalar) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, child, "child", mz_schema_map(mz_schema_string(), on_insert_scalar_ok));
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        k("child") "{"
          kv("k", "v") ","
          kv("k2", "v2")
        "}"
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_ARR_LEN(root_t, child, 2),
        MZ_EXPECT_ARR_ENTRY_CHILD_STR(root_t, child, 0, child_t, key, "k"),
        MZ_EXPECT_ARR_ENTRY_CHILD_STR(root_t, child, 0, child_t, str, "v"),
      },
    },
    {
      .json = "{"
        k("child") "[]"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.child",
    },
    {
      .json = "{"
        k("child") "{"
          kv("k", 1)
        "}"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.child.k",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, map_scalar_empty) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, child, "child", mz_schema_map(mz_schema_string(), on_insert_scalar_ok));
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        k("child") "{"
        "}"
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_ARR_LEN(root_t, child, 0),
      },
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, map_scalar_single_entry) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_BIND(ut.b, root_t, child, "child", mz_schema_map(mz_schema_string(), on_insert_scalar_ok));
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        k("child") "{"
          kv("k", "v")
        "}"
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_ARR_LEN(root_t, child, 1),
        MZ_EXPECT_ARR_ENTRY_CHILD_STR(root_t, child, 0, child_t, key, "k"),
        MZ_EXPECT_ARR_ENTRY_CHILD_STR(root_t, child, 0, child_t, str, "v"),
      },
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, map_object) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_MAP_BIND(ut.b, root_t, child, "child", on_insert_object_ok, MZ_OBJECT_STRICT) {
      MZ_BIND(ut.b, child_t, str, "str", mz_schema_string());
      MZ_BIND(ut.b, child_t, boolean, "boolean", mz_schema_bool());
    }
  }

  mz_parse_case_t cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        k("child") "{"
          q("a") ": {"
            kv("str", "x") ","
            kv("boolean", true)
          "}"
        "}"
      "}",
      .expected = MZ_OK,
      .values = {
        MZ_EXPECT_ARR_LEN(root_t, child, 1),
        MZ_EXPECT_ARR_ENTRY_CHILD_STR(root_t, child, 0, child_t, key, "a"),
        MZ_EXPECT_ARR_ENTRY_CHILD_STR(root_t, child, 0, child_t, str, "x"),
        MZ_EXPECT_ARR_ENTRY_CHILD_BOOL(root_t, child, 0, child_t, boolean, true),
      },
    },
    {
      .json = "{"
        k("child") "{"
          q("a") ": {"
            kv("str", 1) ","
            kv("boolean", true)
          "}"
        "}"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.child.a.str",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), cases);
}

UTEST_F(mz, object_ptr) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_PTR_BIND(ut.b, root_t, s32_ptr, "s32_ptr", mz_schema_s32());
    MZ_OBJECT_PTR_BIND(ut.b, root_t, child, "child", MZ_OBJECT_STRICT) {
      MZ_BIND(ut.b, child_t, str, "str", mz_schema_string());
      MZ_OBJECT_PTR_BIND(ut.b, child_t, ptr, "ptr", MZ_OBJECT_STRICT) {
        MZ_BIND(ut.b, child_t, str, "str", mz_schema_string());
      }
    }
  }

  mz_parse_case_t default_cases[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("s32_ptr", 7) ","
        k("child") "{"
          kv("str", "a") ","
          k("ptr") "{"
            kv("str", "b")
          "}"
        "}"
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_S32(root_t, s32_ptr, 7),
        MZ_EXPECT_CHILD_STR(root_t, child, child_t, str, "a"),
        MZ_EXPECT_GRANDCHILD_STR(root_t, child, child_t, ptr, child_t, str, "b"),
      },
    },
    {
      .json = "{"
        kv("s32_ptr", -13) ","
        k("child") "{"
          kv("str", "m") ","
          k("ptr") "{"
            kv("str", "n")
          "}"
        "}"
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_S32(root_t, s32_ptr, -13),
        MZ_EXPECT_CHILD_STR(root_t, child, child_t, str, "m"),
        MZ_EXPECT_GRANDCHILD_STR(root_t, child, child_t, ptr, child_t, str, "n"),
      },
    },
    {
      .json = "{"
        kv("s32_ptr", "bad") ","
        k("child") "{"
          kv("str", "a") ","
          k("ptr") "{"
            kv("str", "b")
          "}"
        "}"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.s32_ptr",
    },
    {
      .json = "{"
        kv("s32_ptr", 7) ","
        k("child") "{"
          kv("str", "a") ","
          k("ptr") "{"
            kv("str", 1)
          "}"
        "}"
      "}",
      .expected = MZ_ERR_TYPE,
      .expected_path = "root.child.ptr.str",
    },
  };

  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), default_cases);
}

UTEST_F(mz, scalar_ptr_alloc) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_PTR_BIND_ALLOC(ut.b, root_t, s32_ptr, "s32_ptr", mz_schema_s32(), on_resolve_s32_ok);
  }
  mz_parse_case_t custom_ok[MZ_CASE_MAX] = {
    {
      .json = "{"
        kv("s32_ptr", 42)
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_S32(root_t, s32_ptr, 42),
      },
    },
    {
      .json = "{"
        kv("s32_ptr", 69)
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_PTR_S32(root_t, s32_ptr, 69),
      },
    },
  };
  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), custom_ok);
}

UTEST_F(mz, object_ptr_alloc) {
  MZ_SCHEMA(ut.b, MZ_OBJECT_STRICT) {
    MZ_OBJECT_PTR_BIND_ALLOC(ut.b, root_t, child, "child", MZ_OBJECT_STRICT, on_resolve_leaf_ok) {
      MZ_BIND(ut.b, child_t, str, "str", mz_schema_string());
    }
  }
  mz_parse_case_t ptr_obj_custom[MZ_CASE_MAX] = {
    {
      .json = "{"
        k("child") "{"
          kv("str", "ok")
        "}"
      "}",
      .expected = MZ_OK,
      .ptrs = {
        MZ_EXPECT_CHILD_STR(root_t, child, child_t, str, "ok"),
      },
    },
  };
  run_parse_cases(&ur, mz_builder_end(ut.b), sp_alloc_type(root_t), ptr_obj_custom);
}
