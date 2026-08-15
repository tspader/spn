#include "spn_test.h"

#include "dag/dag.h"
#include "graph/identity.h"
#include "paths/paths.h"
#include "target/mutate.h"

#define IDENTITY_TEST_MAX_COPIES 2
#define IDENTITY_TEST_MAX_HEADERS 2
#define IDENTITY_TEST_MAX_PATHS 2
#define IDENTITY_TEST_MAX_OBJECTS 3

typedef struct {
  const c8* from;
  const c8* to;
} identity_copy_t;

typedef struct {
  const c8* sub;
  spn_path_root_t root;
} identity_path_t;

typedef struct {
  const c8* qualified;
  const c8* rev;
  identity_copy_t copies [IDENTITY_TEST_MAX_COPIES];
  identity_path_t headers [IDENTITY_TEST_MAX_HEADERS];
} identity_pkg_t;

typedef struct {
  identity_pkg_t pkg;
  const c8* tag;
  const c8* fn;
  identity_path_t inputs [IDENTITY_TEST_MAX_PATHS];
  identity_path_t outputs [IDENTITY_TEST_MAX_PATHS];
} identity_node_t;

typedef struct {
  bool distinct;
} identity_expect_t;

typedef struct {
  const c8* name;
  identity_pkg_t a;
  identity_pkg_t b;
  identity_expect_t expect;
} identity_pkg_test_t;

typedef struct {
  const c8* name;
  identity_node_t a;
  identity_node_t b;
  identity_expect_t expect;
} identity_node_test_t;

static spn_path_t identity_path(const identity_path_t* spec) {
  return (spn_path_t) { .root = spec->root, .sub = sp_cstr_as_str(spec->sub) };
}

static spn_pkg_unit_t* identity_unit(sp_mem_t mem, const identity_pkg_t* spec) {
  spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
  info->qualified = sp_str_view(spec->qualified);
  sp_da_init(mem, info->publish.copy);
  sp_carr_for(spec->copies, it) {
    if (!spec->copies[it].from) {
      break;
    }
    sp_da_push(info->publish.copy, ((spn_publish_copy_t) {
      .from = sp_str_view(spec->copies[it].from),
      .to = sp_str_view(spec->copies[it].to),
    }));
  }

  spn_target_info_t lib = { .name = sp_cstr_as_str("L"), .kind = SPN_TARGET_KIND_LIB };
  spn_target_info_init(mem, &lib);
  sp_carr_for(spec->headers, it) {
    if (!spec->headers[it].sub) {
      break;
    }
    sp_da_push(lib.headers, identity_path(&spec->headers[it]));
  }
  sp_str_om_init(info->libs);
  sp_str_om_insert(info->libs, lib.name, lib);

  spn_pkg_unit_t* unit = sp_alloc_type(mem, spn_pkg_unit_t);
  unit->info = info;
  unit->source = SPN_PKG_SOURCE_INDEX;
  sp_da_init(mem, unit->user_nodes);
  return unit;
}

static spn_build_source_pin_t identity_pin(const identity_pkg_t* spec) {
  return (spn_build_source_pin_t) {
    .kind = SPN_PKG_ROOT_GIT,
    .rev = sp_str_view(spec->rev),
  };
}

static spn_user_node_t* identity_node(sp_mem_t mem, const identity_node_t* spec) {
  spn_user_node_t* node = sp_alloc_type(mem, spn_user_node_t);
  node->pkg = identity_unit(mem, &spec->pkg);
  node->tag = sp_str_view(spec->tag);
  node->fn = sp_str_view(spec->fn);
  sp_da_init(mem, node->inputs);
  sp_da_init(mem, node->outputs);
  sp_carr_for(spec->inputs, it) {
    if (!spec->inputs[it].sub) {
      break;
    }
    sp_da_push(node->inputs, identity_path(&spec->inputs[it]));
  }
  sp_carr_for(spec->outputs, it) {
    if (!spec->outputs[it].sub) {
      break;
    }
    sp_da_push(node->outputs, identity_path(&spec->outputs[it]));
  }
  return node;
}

static sp_err_t identity_expect_distinct(sp_test_t* t, spn_dag_digest_t a, spn_dag_digest_t b, const identity_expect_t* expect) {
  sp_expect_eq(t, expect->distinct, !spn_dag_digest_equal(a, b));
  return SP_OK;
}

static const identity_pkg_test_t tree_tests [] = {
  {
    .name = "identical_units_agree",
    .a = { .qualified = "A", .rev = "1", .copies = { { "source/H", "include" } } },
    .b = { .qualified = "A", .rev = "1", .copies = { { "source/H", "include" } } },
  },
  {
    .name = "distinct_qualified",
    .a = { .qualified = "A", .rev = "1" },
    .b = { .qualified = "B", .rev = "1" },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_publish_copy",
    .a = { .qualified = "A", .rev = "1", .copies = { { "source/H", "include" } } },
    .b = { .qualified = "A", .rev = "1", .copies = { { "source/I", "include" } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_pinned_source",
    .a = { .qualified = "A", .rev = "1", .copies = { { "source/H", "include" } } },
    .b = { .qualified = "A", .rev = "2", .copies = { { "source/H", "include" } } },
    .expect = { .distinct = true }
  },
  {
    .name = "identical_headers_agree",
    .a = { .qualified = "A", .rev = "1", .headers = { { "H", SPN_PATH_ROOT_CHECKOUT }, { "I", SPN_PATH_ROOT_PROJECT } } },
    .b = { .qualified = "A", .rev = "1", .headers = { { "H", SPN_PATH_ROOT_CHECKOUT }, { "I", SPN_PATH_ROOT_PROJECT } } },
  },
  {
    .name = "distinct_header_sub",
    .a = { .qualified = "A", .rev = "1", .headers = { { "H", SPN_PATH_ROOT_CHECKOUT } } },
    .b = { .qualified = "A", .rev = "1", .headers = { { "I", SPN_PATH_ROOT_CHECKOUT } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_header_root",
    .a = { .qualified = "A", .rev = "1", .headers = { { "H", SPN_PATH_ROOT_CHECKOUT } } },
    .b = { .qualified = "A", .rev = "1", .headers = { { "H", SPN_PATH_ROOT_PROJECT } } },
    .expect = { .distinct = true }
  },
};

sp_test_each(identity, tree, identity_pkg_test_t, tree_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_build_source_pin_t pin_a = identity_pin(&it->a);
  spn_build_source_pin_t pin_b = identity_pin(&it->b);
  spn_dag_digest_t a = spn_build_tree_identity(identity_unit(mem, &it->a), &pin_a);
  spn_dag_digest_t b = spn_build_tree_identity(identity_unit(mem, &it->b), &pin_b);
  return identity_expect_distinct(t, a, b, &it->expect);
}

static const identity_pkg_test_t package_tests [] = {
  {
    .name = "identical_units_agree",
    .a = { .qualified = "A", .rev = "1", .copies = { { "source/H", "store/H" } } },
    .b = { .qualified = "A", .rev = "1", .copies = { { "source/H", "store/H" } } },
  },
  {
    .name = "distinct_publish_copy",
    .a = { .qualified = "A", .rev = "1", .copies = { { "source/H", "store/H" } } },
    .b = { .qualified = "A", .rev = "1", .copies = { { "source/I", "store/I" } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_pinned_source",
    .a = { .qualified = "A", .rev = "1", .copies = { { "source/H", "store/H" } } },
    .b = { .qualified = "A", .rev = "2", .copies = { { "source/H", "store/H" } } },
    .expect = { .distinct = true }
  },
};

sp_test_each(identity, package, identity_pkg_test_t, package_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_build_source_pin_t pin_a = identity_pin(&it->a);
  spn_build_source_pin_t pin_b = identity_pin(&it->b);
  spn_dag_digest_t a = spn_build_package_identity(identity_unit(mem, &it->a), &pin_a);
  spn_dag_digest_t b = spn_build_package_identity(identity_unit(mem, &it->b), &pin_b);
  return identity_expect_distinct(t, a, b, &it->expect);
}

static const identity_node_test_t user_tests [] = {
  {
    .name = "identical_nodes_agree",
    .a = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .inputs = { { "A-1/H", SPN_PATH_ROOT_CHECKOUT } } },
    .b = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .inputs = { { "A-1/H", SPN_PATH_ROOT_CHECKOUT } } },
  },
  {
    .name = "distinct_input_sub",
    .a = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .inputs = { { "A-1/H", SPN_PATH_ROOT_CHECKOUT } } },
    .b = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .inputs = { { "A-1/I", SPN_PATH_ROOT_CHECKOUT } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_input_root",
    .a = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .inputs = { { "A-1/H", SPN_PATH_ROOT_CHECKOUT } } },
    .b = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .inputs = { { "A-1/H", SPN_PATH_ROOT_PROJECT } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_outputs",
    .a = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .outputs = { { "A-1/H", SPN_PATH_ROOT_BUILD } } },
    .b = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F", .outputs = { { "A-1/I", SPN_PATH_ROOT_BUILD } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_pinned_source",
    .a = { .pkg = { .qualified = "A", .rev = "1" }, .tag = "N", .fn = "F" },
    .b = { .pkg = { .qualified = "A", .rev = "2" }, .tag = "N", .fn = "F" },
    .expect = { .distinct = true }
  },
};

sp_test_each(identity, user, identity_node_test_t, user_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_build_source_pin_t pin_a = identity_pin(&it->a.pkg);
  spn_build_source_pin_t pin_b = identity_pin(&it->b.pkg);
  spn_dag_digest_t a = spn_build_user_identity(identity_node(mem, &it->a), &pin_a);
  spn_dag_digest_t b = spn_build_user_identity(identity_node(mem, &it->b), &pin_b);
  return identity_expect_distinct(t, a, b, &it->expect);
}

typedef struct {
  identity_path_t output;
  identity_path_t objects [IDENTITY_TEST_MAX_OBJECTS];
} identity_link_t;

typedef struct {
  const c8* name;
  identity_link_t a;
  identity_link_t b;
  identity_expect_t expect;
} identity_link_test_t;

static const identity_link_test_t link_tests [] = {
  {
    .name = "identical_links_agree",
    .a = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD } } },
    .b = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD } } },
  },
  {
    .name = "distinct_object_sub",
    .a = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD } } },
    .b = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "B.o", SPN_PATH_ROOT_BUILD } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_object_root",
    .a = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD } } },
    .b = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_CACHE } } },
    .expect = { .distinct = true }
  },
  {
    .name = "extra_object",
    .a = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD } } },
    .b = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD }, { "B.o", SPN_PATH_ROOT_BUILD } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_output",
    .a = { .output = { "L", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD } } },
    .b = { .output = { "M", SPN_PATH_ROOT_BUILD }, .objects = { { "A.o", SPN_PATH_ROOT_BUILD } } },
    .expect = { .distinct = true }
  },
};

static sp_err_t identity_link_digest(sp_test_t* t, sp_mem_t mem, const identity_link_t* spec, spn_dag_digest_t* digest) {
  spn_toolchain_unit_t toolchain = {
    .cc = {
      .name = sp_str_lit("T"),
      .driver = SPN_CC_DRIVER_GCC,
      .compiler = { .program = spn_arg_lit(sp_str_lit("cc")) },
    },
  };
  spn_build_unit_t build = {
    .toolchain = &toolchain,
    .profile = { .arch = SPN_ARCH_X64, .os = SPN_OS_LINUX, .abi = SPN_ABI_GNU },
  };
  spn_pkg_unit_t pkg = { .build = &build };
  pkg.paths.work = (spn_path_t) { .root = SPN_PATH_ROOT_BUILD, .sub = sp_str_lit("W") };
  spn_target_unit_t target = {
    .pkg = &pkg,
    .kind = SPN_CC_OUTPUT_EXE,
    .link = { .cc = { .lang = SPN_LANG_C, .kind = SPN_CC_OUTPUT_EXE } },
  };

  sp_da(spn_path_t) objects = sp_da_new(mem, spn_path_t);
  sp_carr_for(spec->objects, it) {
    if (!spec->objects[it].sub) {
      break;
    }
    sp_da_push(objects, identity_path(&spec->objects[it]));
  }

  spn_path_t output = identity_path(&spec->output);
  sp_must_eq(t, SPN_OK, spn_build_link_identity(mem, &target, output, objects, sp_zero_s(spn_path_t), digest));
  return SP_OK;
}

sp_test_each(identity, link, identity_link_test_t, link_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_dag_digest_t a = sp_zero;
  spn_dag_digest_t b = sp_zero;
  sp_try(identity_link_digest(t, mem, &it->a, &a));
  sp_try(identity_link_digest(t, mem, &it->b, &b));
  return identity_expect_distinct(t, a, b, &it->expect);
}
