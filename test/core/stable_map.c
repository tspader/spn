#include "spn_test.h"

#include "sp_om/sp_om.h"


// ---- s32 key tests (flat scalar key, default hash/compare) ----

sp_test(sm_s32, insert_and_get) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 1, 10);
  sp_om_insert(map, 2, 20);
  sp_om_insert(map, 3, 30);

  sp_must_eq(t, sp_om_size(map), 3);
  sp_must_eq(t, *sp_om_get(map, 1), 10);
  sp_must_eq(t, *sp_om_get(map, 2), 20);
  sp_must_eq(t, *sp_om_get(map, 3), 30);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, insertion_order) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 100, 10);
  sp_om_insert(map, 200, 20);
  sp_om_insert(map, 300, 30);

  sp_must_eq(t, *sp_om_at(map, 0), 10);
  sp_must_eq(t, *sp_om_at(map, 1), 20);
  sp_must_eq(t, *sp_om_at(map, 2), 30);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, iteration) {
  sp_om(s32, s32) map = SP_NULLPTR;

  for (s32 i = 0; i < 5; i++) {
    sp_om_insert(map, i, i * 10);
  }

  s32 expected = 0;
  sp_om_for(map, it) {
    sp_must_eq(t, *sp_om_at(map, it), expected);
    expected += 10;
  }

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, has_key) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_must(t, !sp_om_has(map, 42));

  sp_om_insert(map, 42, 100);

  sp_must(t, sp_om_has(map, 42));
  sp_must(t, !sp_om_has(map, 99));

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, stable_pointers) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 0, 100);
  s32* first = sp_om_at(map, 0);

  for (s32 i = 1; i <= 100; i++) {
    sp_om_insert(map, i, i);
  }

  sp_must_eq(t, *first, 100);
  sp_must_eq(t, first, sp_om_at(map, 0));

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, duplicate_key_insert) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 7, 100);
  sp_om_insert(map, 7, 200);
  sp_om_insert(map, 7, 300);

  sp_must_eq(t, sp_om_size(map), 1);
  sp_must_eq(t, *sp_om_get(map, 7), 100);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, getp_and_missing_key) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 5, 42);

  s32** pp = sp_om_getp(map, 5);
  sp_must(t, pp != SP_NULLPTR);
  sp_must_eq(t, **pp, 42);

  s32** missing_pp = sp_om_getp(map, 999);
  sp_must(t, missing_pp == SP_NULLPTR);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, null_map_operations) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_must_eq(t, sp_om_size(map), 0);
  sp_must(t, sp_om_empty(map));
  sp_must(t, sp_om_getp(map, 1) == SP_NULLPTR);
  sp_must(t, !sp_om_has(map, 1));

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, empty) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_must(t, sp_om_empty(map));

  sp_om_insert(map, 1, 10);
  sp_must(t, !sp_om_empty(map));

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, back) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 1, 10);
  sp_must_eq(t, *sp_om_back(map), 10);

  sp_om_insert(map, 2, 20);
  sp_must_eq(t, *sp_om_back(map), 20);

  sp_om_insert(map, 3, 30);
  sp_must_eq(t, *sp_om_back(map), 30);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, zero_key) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 0, 42);

  sp_must(t, sp_om_has(map, 0));
  sp_must_eq(t, *sp_om_get(map, 0), 42);
  sp_must_eq(t, sp_om_size(map), 1);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, stable_pointers_via_get) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 0, 100);
  s32* via_get = sp_om_get(map, 0);

  for (s32 i = 1; i <= 100; i++) {
    sp_om_insert(map, i, i);
  }

  sp_must_eq(t, *via_get, 100);
  sp_must_eq(t, via_get, sp_om_get(map, 0));

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, interleaved_insert_and_lookup) {
  sp_om(s32, s32) map = SP_NULLPTR;

  sp_om_insert(map, 1, 10);
  sp_must_eq(t, *sp_om_get(map, 1), 10);

  sp_om_insert(map, 2, 20);
  sp_must_eq(t, *sp_om_get(map, 1), 10);
  sp_must_eq(t, *sp_om_get(map, 2), 20);

  sp_om_insert(map, 3, 30);
  sp_must_eq(t, *sp_om_get(map, 1), 10);
  sp_must_eq(t, *sp_om_get(map, 2), 20);
  sp_must_eq(t, *sp_om_get(map, 3), 30);
  sp_must_eq(t, sp_om_size(map), 3);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_s32, many_entries_preserve_order) {
  sp_om(s32, s32) map = SP_NULLPTR;

  for (s32 i = 0; i < 200; i++) {
    sp_om_insert(map, i, i * 10);
  }

  sp_must_eq(t, sp_om_size(map), 200);

  sp_om_for(map, it) {
    sp_must_eq(t, *sp_om_at(map, it), (s32)it * 10);
  }

  sp_om_free(map);
  return SP_OK;
}

// ---- compound struct key tests (the whole point) ----

typedef struct {
  s32 a;
  s32 b;
} pair_key_t;

sp_test(sm_pair, insert_and_get) {
  sp_om(pair_key_t, s32) map = SP_NULLPTR;

  sp_om_insert(map, ((pair_key_t){.a = 1, .b = 2}), 10);
  sp_om_insert(map, ((pair_key_t){.a = 3, .b = 4}), 20);

  sp_must_eq(t, sp_om_size(map), 2);
  sp_must_eq(t, *sp_om_get(map, ((pair_key_t){.a = 1, .b = 2})), 10);
  sp_must_eq(t, *sp_om_get(map, ((pair_key_t){.a = 3, .b = 4})), 20);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_pair, different_keys_same_fields) {
  sp_om(pair_key_t, s32) map = SP_NULLPTR;

  sp_om_insert(map, ((pair_key_t){.a = 1, .b = 2}), 10);
  sp_om_insert(map, ((pair_key_t){.a = 2, .b = 1}), 20);

  sp_must_eq(t, sp_om_size(map), 2);
  sp_must_eq(t, *sp_om_get(map, ((pair_key_t){.a = 1, .b = 2})), 10);
  sp_must_eq(t, *sp_om_get(map, ((pair_key_t){.a = 2, .b = 1})), 20);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_pair, duplicate_compound_key) {
  sp_om(pair_key_t, s32) map = SP_NULLPTR;

  sp_om_insert(map, ((pair_key_t){.a = 5, .b = 6}), 100);
  sp_om_insert(map, ((pair_key_t){.a = 5, .b = 6}), 200);

  sp_must_eq(t, sp_om_size(map), 1);
  sp_must_eq(t, *sp_om_get(map, ((pair_key_t){.a = 5, .b = 6})), 100);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_pair, has_compound_key) {
  sp_om(pair_key_t, s32) map = SP_NULLPTR;

  sp_om_insert(map, ((pair_key_t){.a = 1, .b = 1}), 42);

  sp_must(t, sp_om_has(map, ((pair_key_t){.a = 1, .b = 1})));
  sp_must(t, !sp_om_has(map, ((pair_key_t){.a = 1, .b = 2})));
  sp_must(t, !sp_om_has(map, ((pair_key_t){.a = 2, .b = 1})));

  sp_om_free(map);
  return SP_OK;
}

// ---- struct value with struct key ----

typedef struct {
  sp_str_t name;
  u32 version;
  sp_str_t triple;
} pkg_key_t;

typedef struct {
  sp_str_t path;
  bool loaded;
  s32 priority;
} pkg_value_t;

typedef struct {
  pkg_key_t key;
  pkg_value_t value;
} kvp_t;

#define LOADED true
#define UNLOADED false
#define LOW_PRIORITY 1
#define MID_PRIORITY 2
#define HIGH_PRIORITY 3


static sp_hash_t pkg_key_hash(void* key, u64 size) {
  (void)size;
  pkg_key_t* k = (pkg_key_t*)key;
  sp_hash_t h = sp_hash_bytes(k->name.data, k->name.len, 0);
  h ^= sp_hash_bytes(&k->version, sizeof(k->version), 0);
  h ^= sp_hash_bytes(k->triple.data, k->triple.len, 0);
  return h;
}

static bool pkg_key_compare(void* ka, void* kb, u64 size) {
  (void)size;
  pkg_key_t* a = (pkg_key_t*)ka;
  pkg_key_t* b = (pkg_key_t*)kb;
  return sp_str_equal(a->name, b->name)
      && a->version == b->version
      && sp_str_equal(a->triple, b->triple);
}

sp_test(sm_pkg, custom_hash_compare) {
  sp_om(pkg_key_t, pkg_value_t) map = SP_NULLPTR;
  sp_om_set_fns(map, pkg_key_hash, pkg_key_compare);

  kvp_t k [] = {
    {
      .key = { strl("sqlite"), 350, strl("x86_64-linux-gnu") },
      .value = { strl("/usr/lib/sqlite350-x86"), LOADED, LOW_PRIORITY }
    },
    {
      .key = { strl("sqlite"), 360, strl("x86_64-linux-gnu") },
      .value = { strl("/usr/lib/sqlite350-x86"), UNLOADED, MID_PRIORITY }
    },
    {
      .key = { strl("sqlite"), 350, strl("aarch64-linux-gnu") },
      .value = { strl("/usr/lib/sqlite350-arm"), LOADED, HIGH_PRIORITY }
    }
  };

  sp_carr_for(k, it) {
    sp_om_insert(map, k[it].key, k[it].value);
  }

  sp_must_eq(t, sp_om_size(map), 3);

  pkg_value_t* v1 = sp_om_get(map, k[0].key);
  sp_must(t, v1->loaded);
  sp_must_eq(t, v1->priority, 1);

  pkg_value_t* v2 = sp_om_get(map, k[1].key);
  sp_must(t, !v2->loaded);
  sp_must_eq(t, v2->priority, 2);

  pkg_value_t* v3 = sp_om_get(map, k[2].key);
  sp_must(t, v3->loaded);
  sp_must_eq(t, v3->priority, 3);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_pkg, same_name_different_version_and_triple) {
  sp_om(pkg_key_t, pkg_value_t) map = SP_NULLPTR;
  sp_om_set_fns(map, pkg_key_hash, pkg_key_compare);

  pkg_key_t keys[] = {
    { strl("zlib"), 100, strl("x86_64-linux-gnu") },
    { strl("zlib"), 100, strl("aarch64-linux-gnu") },
    { strl("zlib"), 200, strl("x86_64-linux-gnu") },
    { strl("zlib"), 200, strl("aarch64-linux-gnu") },
  };

  for (s32 i = 0; i < 4; i++) {
    sp_om_insert(map, keys[i], ((pkg_value_t){ .path = strl(""), .loaded = false, .priority = i }));
  }

  sp_must_eq(t, sp_om_size(map), 4);

  for (s32 i = 0; i < 4; i++) {
    pkg_value_t* v = sp_om_get(map, keys[i]);
    sp_must(t, v != SP_NULLPTR);
    sp_must_eq(t, v->priority, i);
  }

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_pkg, duplicate_with_custom_fns) {
  sp_om(pkg_key_t, pkg_value_t) map = SP_NULLPTR;
  sp_om_set_fns(map, pkg_key_hash, pkg_key_compare);

  pkg_key_t k = { strl("openssl"), 111, strl("x86_64-linux-gnu") };

  sp_om_insert(map, k, ((pkg_value_t){ .path = strl("/first"), .loaded = true, .priority = 1 }));
  sp_om_insert(map, k, ((pkg_value_t){ .path = strl("/second"), .loaded = false, .priority = 2 }));

  sp_must_eq(t, sp_om_size(map), 1);
  pkg_value_t* v = sp_om_get(map, k);
  sp_must_eq(t, v->priority, 1);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_pkg, stable_pointers_with_custom_fns) {
  sp_mem_t mem = sp_test_arena(t);
  sp_om(pkg_key_t, pkg_value_t) map = SP_NULLPTR;
  sp_om_set_fns(map, pkg_key_hash, pkg_key_compare);

  pkg_key_t k0 = { .name = sp_str_lit("first"), .version = 1, .triple = sp_str_lit("x86_64") };
  sp_om_insert(map, k0, ((pkg_value_t){ .path = sp_str_lit("/first"), .loaded = true, .priority = 0 }));
  pkg_value_t* first = sp_om_at(map, 0);

  for (s32 i = 1; i <= 100; i++) {
    sp_str_t name = sp_fmt(mem, "pkg{}", sp_fmt_int(i)).value;
    pkg_key_t k = { .name = name, .version = (u32)i, .triple = sp_str_lit("x86_64") };
    sp_om_insert(map, k, ((pkg_value_t){ .path = sp_str_lit(""), .loaded = false, .priority = i }));
  }

  sp_must_eq(t, first, sp_om_at(map, 0));
  sp_must_eq(t, first->priority, 0);
  sp_must(t, first->loaded);

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_pkg, iteration_order_preserved) {
  sp_mem_t mem = sp_test_arena(t);
  sp_om(pkg_key_t, pkg_value_t) map = SP_NULLPTR;
  sp_om_set_fns(map, pkg_key_hash, pkg_key_compare);

  for (s32 i = 0; i < 10; i++) {
    sp_str_t name = sp_fmt(mem, "pkg{}", sp_fmt_int(i)).value;
    pkg_key_t k = { .name = name, .version = (u32)i, .triple = sp_str_lit("any") };
    sp_om_insert(map, k, ((pkg_value_t){ .path = sp_str_lit(""), .loaded = false, .priority = i }));
  }

  sp_must_eq(t, sp_om_size(map), 10);
  sp_om_for(map, it) {
    sp_must_eq(t, sp_om_at(map, it)->priority, (s32)it);
  }

  sp_om_free(map);
  return SP_OK;
}

// ---- u64 key, different value type ----

typedef struct {
  f64 x;
  f64 y;
  f64 z;
} vec3_t;

sp_test(sm_u64, u64_key_struct_value) {
  sp_om(u64, vec3_t) map = SP_NULLPTR;

  sp_om_insert(map, (u64)1000, ((vec3_t){ .x = 1.0, .y = 2.0, .z = 3.0 }));
  sp_om_insert(map, (u64)2000, ((vec3_t){ .x = 4.0, .y = 5.0, .z = 6.0 }));

  vec3_t* v = sp_om_get(map, (u64)1000);
  sp_must(t, v != SP_NULLPTR);
  sp_must_eq(t, v->x, 1.0);
  sp_must_eq(t, v->y, 2.0);
  sp_must_eq(t, v->z, 3.0);

  sp_must(t, !sp_om_has(map, (u64)9999));

  sp_om_free(map);
  return SP_OK;
}

sp_test(sm_u64, many_entries) {
  sp_om(u64, vec3_t) map = SP_NULLPTR;

  for (u64 i = 0; i < 200; i++) {
    sp_om_insert(map, i, ((vec3_t){ .x = (f64)i, .y = (f64)(i * 2), .z = (f64)(i * 3) }));
  }

  sp_must_eq(t, sp_om_size(map), 200);

  for (u64 i = 0; i < 200; i++) {
    vec3_t* v = sp_om_get(map, i);
    sp_must(t, v != SP_NULLPTR);
    sp_must_eq(t, v->x, (f64)i);
  }

  sp_om_free(map);
  return SP_OK;
}

// ---- sp_str_om (string-keyed specialization) ----

sp_test(om, insert_and_get) {
  sp_str_om(s32) map = SP_NULLPTR;

  sp_str_om_insert(map, strl("a"), 1);
  sp_str_om_insert(map, strl("b"), 2);
  sp_str_om_insert(map, strl("c"), 3);

  sp_must_eq(t, sp_str_om_size(map), 3);
  sp_must_eq(t, *sp_str_om_get(map, strl("a")), 1);
  sp_must_eq(t, *sp_str_om_get(map, strl("b")), 2);
  sp_must_eq(t, *sp_str_om_get(map, strl("c")), 3);

  sp_str_om_free(map);
  return SP_OK;
}

sp_test(om, insertion_order) {
  sp_str_om(s32) map = SP_NULLPTR;

  sp_str_om_insert(map, strl("x"), 10);
  sp_str_om_insert(map, strl("y"), 20);
  sp_str_om_insert(map, strl("z"), 30);

  sp_must_eq(t, *sp_str_om_at(map, 0), 10);
  sp_must_eq(t, *sp_str_om_at(map, 1), 20);
  sp_must_eq(t, *sp_str_om_at(map, 2), 30);

  sp_str_om_free(map);
  return SP_OK;
}

sp_test(om, duplicate_key) {
  sp_str_om(s32) map = SP_NULLPTR;

  sp_str_om_insert(map, strl("key"), 100);
  sp_str_om_insert(map, strl("key"), 200);

  sp_must_eq(t, sp_str_om_size(map), 1);
  sp_must_eq(t, *sp_str_om_get(map, strl("key")), 100);

  sp_str_om_free(map);
  return SP_OK;
}

sp_test(om, has_key) {
  sp_str_om(s32) map = SP_NULLPTR;

  sp_must(t, !sp_str_om_has(map, strl("foo")));

  sp_str_om_insert(map, strl("foo"), 42);

  sp_must(t, sp_str_om_has(map, strl("foo")));
  sp_must(t, !sp_str_om_has(map, strl("bar")));

  sp_str_om_free(map);
  return SP_OK;
}

sp_test(om, stable_pointers) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_om(s32) map = SP_NULLPTR;

  sp_str_om_insert(map, strl("first"), 100);
  s32* first = sp_str_om_at(map, 0);

  for (s32 i = 0; i < 100; i++) {
    sp_str_t key = sp_fmt(mem, "key{}", sp_fmt_int(i)).value;
    sp_str_om_insert(map, key, i);
  }

  sp_must_eq(t, *first, 100);
  sp_must_eq(t, first, sp_str_om_at(map, 0));

  sp_str_om_free(map);
  return SP_OK;
}

sp_test(om, struct_values) {
  typedef struct { s32 x; s32 y; } point_t;
  sp_str_om(point_t) map = SP_NULLPTR;

  sp_str_om_insert(map, strl("origin"), ((point_t){.x = 0, .y = 0}));
  sp_str_om_insert(map, strl("point1"), ((point_t){.x = 10, .y = 20}));

  sp_must_eq(t, sp_str_om_size(map), 2);

  point_t* origin = sp_str_om_get(map, strl("origin"));
  sp_must_eq(t, origin->x, 0);
  sp_must_eq(t, origin->y, 0);

  point_t* p1 = sp_str_om_at(map, 1);
  sp_must_eq(t, p1->x, 10);
  sp_must_eq(t, p1->y, 20);

  sp_str_om_free(map);
  return SP_OK;
}
