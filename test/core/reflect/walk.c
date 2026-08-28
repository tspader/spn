#include "spn_test.h"

#include "reflect/reflect.h"

#define REFLECT_TEST_MAX_TAGS 4
#define REFLECT_TEST_MAX_ITEMS 4

typedef struct {
  sp_str_t name;
  sp_da(sp_str_t) tags;
} item_t;

typedef struct {
  sp_str_t title;
  item_t head;
  sp_da(item_t) items;
} doc_t;

static const spn_reflect_field_t item_fields [] = {
  { .key = "name", .offset = spn_reflect_offset(item_t, name), .kind = SPN_REFLECT_STR },
  { .key = "tags", .offset = spn_reflect_offset(item_t, tags), .kind = SPN_REFLECT_STR_ARRAY },
};

static const spn_reflect_type_t item_type = {
  .size = sizeof(item_t),
  .fields = item_fields,
  .num_fields = sp_carr_len(item_fields),
};

static const spn_reflect_field_t doc_fields [] = {
  { .key = "title", .offset = spn_reflect_offset(doc_t, title), .kind = SPN_REFLECT_STR },
  { .key = "head", .offset = spn_reflect_offset(doc_t, head), .kind = SPN_REFLECT_OBJECT, .object = &item_type },
  { .key = "items", .offset = spn_reflect_offset(doc_t, items), .kind = SPN_REFLECT_OBJECT_ARRAY, .object = &item_type },
};

static const spn_reflect_type_t doc_type = {
  .size = sizeof(doc_t),
  .fields = doc_fields,
  .num_fields = sp_carr_len(doc_fields),
};

typedef struct {
  const c8* name;
  const c8* tags [REFLECT_TEST_MAX_TAGS];
} item_expect_t;

typedef struct {
  spn_err_t err;
  const c8* title;
  item_expect_t head;
  item_expect_t items [REFLECT_TEST_MAX_ITEMS];
} expect_t;

typedef struct {
  const c8* name;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "full",
    .expect = {
      .title = "A",
      .head = { .name = "B", .tags = { "C" } },
      .items = {
        { .name = "D", .tags = { "E", "F" } },
        { .name = "G" },
        { .name = "", .tags = { "H", "\xc5\xba" } },
      },
    },
  },
  {
    .name = "mismatch",
    .expect = {
      .err = SPN_ERROR,
    },
  },
  {
    .name = "invalid",
    .expect = {
      .err = SPN_ERROR,
    },
  },
};

static void expect_item(sp_test_t* t, const item_t* item, const item_expect_t* expect) {
  sp_expect_str_eq_c(t, item->name, expect->name);
  sp_expect_strs_eq(t, item->tags, sp_da_size(item->tags), expect->tags);
}

sp_test_each(reflect_walk, docs, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = test_repo_path(mem, sp_fmt(mem, "test/core/reflect/walk/{}.json", sp_fmt_cstr(it->name)).value);
  sp_str_t json = sp_zero;
  sp_must(t, !sp_io_read_file(mem, path, &json));

  doc_t doc = sp_zero;
  sp_must_eq(t, it->expect.err, spn_reflect_json_read(json, &doc_type, &doc, mem));
  if (it->expect.err) {
    return SP_OK;
  }

  sp_expect_str_eq_c(t, doc.title, it->expect.title);
  expect_item(t, &doc.head, &it->expect.head);

  u32 len = 0;
  sp_carr_detect_len(it->expect.items, len, it->expect.items[len].name);
  sp_must_eq(t, len, sp_da_size(doc.items));
  sp_for(at, len) {
    expect_item(t, &doc.items[at], &it->expect.items[at]);
  }
  return SP_OK;
}
