#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "sp/sp_om.h"
#include "unit/types.h"

UTEST_MAIN()

UTEST(unit, context_qualified_identity) {
  spn_pkg_id_t pkg = { .qualified = 69 };
  spn_pkg_unit_id_t pkg_a = { .pkg = pkg, .ctx = 0 };
  spn_pkg_unit_id_t pkg_b = { .pkg = pkg, .ctx = 1 };
  spn_target_unit_id_t target_a = { .pkg = pkg_a, .target = 69 };
  spn_target_unit_id_t target_b = { .pkg = pkg_b, .target = 69 };
  spn_compile_unit_id_t object_a = { .target = target_a, .source = 69 };
  spn_compile_unit_id_t object_b = { .target = target_b, .source = 69 };

  sp_om(spn_pkg_unit_id_t, const c8*) packages = SP_NULLPTR;
  sp_om(spn_target_unit_id_t, const c8*) targets = SP_NULLPTR;
  sp_om(spn_compile_unit_id_t, const c8*) objects = SP_NULLPTR;

  sp_om_insert(packages, pkg_a, "test");
  sp_om_insert(packages, pkg_b, "test");
  sp_om_insert(targets, target_a, "spum");
  sp_om_insert(targets, target_b, "spum");
  sp_om_insert(objects, object_a, "main");
  sp_om_insert(objects, object_b, "main");

  EXPECT_EQ(2, sp_om_size(packages));
  EXPECT_EQ(2, sp_om_size(targets));
  EXPECT_EQ(2, sp_om_size(objects));

  sp_om_free(packages);
  sp_om_free(targets);
  sp_om_free(objects);
}
