#include "spn_test.h"

#include "dag/dag.h"
#include "task/build/identity.h"

#define IDENTITY_TEST_MAX_COPIES 2
#define IDENTITY_TEST_MAX_STRS 2

typedef struct {
  const c8* from;
  const c8* to;
} identity_copy_t;

typedef struct {
  const c8* qualified;
  const c8* source;
  identity_copy_t copies [IDENTITY_TEST_MAX_COPIES];
} identity_pkg_t;

typedef struct {
  identity_pkg_t pkg;
  const c8* tag;
  const c8* fn;
  const c8* inputs [IDENTITY_TEST_MAX_STRS];
  const c8* outputs [IDENTITY_TEST_MAX_STRS];
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

static const spn_dag_roots_t identity_roots = {
  .dirs = {
    [SPN_DAG_ROOT_CHECKOUT] = { .data = "/C", .len = 2 },
  }
};

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

  spn_pkg_unit_t* unit = sp_alloc_type(mem, spn_pkg_unit_t);
  unit->info = info;
  unit->source = SPN_PKG_SOURCE_INDEX;
  unit->paths.source = sp_str_view(spec->source);
  sp_da_init(mem, unit->user_nodes);
  return unit;
}

static spn_user_node_t* identity_node(sp_mem_t mem, const identity_node_t* spec) {
  spn_user_node_t* node = sp_alloc_type(mem, spn_user_node_t);
  node->pkg = identity_unit(mem, &spec->pkg);
  node->tag = sp_str_view(spec->tag);
  node->fn = sp_str_view(spec->fn);
  sp_da_init(mem, node->inputs);
  sp_da_init(mem, node->outputs);
  sp_carr_for(spec->inputs, it) {
    if (!spec->inputs[it]) {
      break;
    }
    sp_da_push(node->inputs, sp_str_view(spec->inputs[it]));
  }
  sp_carr_for(spec->outputs, it) {
    if (!spec->outputs[it]) {
      break;
    }
    sp_da_push(node->outputs, sp_str_view(spec->outputs[it]));
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
    .a = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "include" } } },
    .b = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "include" } } },
  },
  {
    .name = "distinct_qualified",
    .a = { .qualified = "A", .source = "/C/A-1" },
    .b = { .qualified = "B", .source = "/C/A-1" },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_publish_copy",
    .a = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "include" } } },
    .b = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/I", "include" } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_pinned_source",
    .a = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "include" } } },
    .b = { .qualified = "A", .source = "/C/A-2", .copies = { { "source/H", "include" } } },
    .expect = { .distinct = true }
  },
};

sp_test_each(identity, tree, identity_pkg_test_t, tree_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_dag_digest_t a = spn_build_tree_identity(&identity_roots, identity_unit(mem, &it->a));
  spn_dag_digest_t b = spn_build_tree_identity(&identity_roots, identity_unit(mem, &it->b));
  return identity_expect_distinct(t, a, b, &it->expect);
}

static const identity_pkg_test_t package_tests [] = {
  {
    .name = "identical_units_agree",
    .a = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "store/H" } } },
    .b = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "store/H" } } },
  },
  {
    .name = "distinct_publish_copy",
    .a = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "store/H" } } },
    .b = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/I", "store/I" } } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_pinned_source",
    .a = { .qualified = "A", .source = "/C/A-1", .copies = { { "source/H", "store/H" } } },
    .b = { .qualified = "A", .source = "/C/A-2", .copies = { { "source/H", "store/H" } } },
    .expect = { .distinct = true }
  },
};

sp_test_each(identity, package, identity_pkg_test_t, package_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_dag_digest_t a = spn_build_package_identity(identity_unit(mem, &it->a));
  spn_dag_digest_t b = spn_build_package_identity(identity_unit(mem, &it->b));
  return identity_expect_distinct(t, a, b, &it->expect);
}

static const identity_node_test_t user_tests [] = {
  {
    .name = "identical_nodes_agree",
    .a = { .pkg = { .qualified = "A", .source = "/C/A-1" }, .tag = "N", .fn = "F", .inputs = { "/C/A-1/H" } },
    .b = { .pkg = { .qualified = "A", .source = "/C/A-1" }, .tag = "N", .fn = "F", .inputs = { "/C/A-1/H" } },
  },
  {
    .name = "distinct_inputs",
    .a = { .pkg = { .qualified = "A", .source = "/C/A-1" }, .tag = "N", .fn = "F", .inputs = { "/C/A-1/H" } },
    .b = { .pkg = { .qualified = "A", .source = "/C/A-1" }, .tag = "N", .fn = "F", .inputs = { "/C/A-1/I" } },
    .expect = { .distinct = true }
  },
  {
    .name = "distinct_pinned_source",
    .a = { .pkg = { .qualified = "A", .source = "/C/A-1" }, .tag = "N", .fn = "F" },
    .b = { .pkg = { .qualified = "A", .source = "/C/A-2" }, .tag = "N", .fn = "F" },
    .expect = { .distinct = true }
  },
};

sp_test_each(identity, user, identity_node_test_t, user_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_dag_digest_t a = spn_build_user_identity(&identity_roots, identity_node(mem, &it->a));
  spn_dag_digest_t b = spn_build_user_identity(&identity_roots, identity_node(mem, &it->b));
  return identity_expect_distinct(t, a, b, &it->expect);
}
