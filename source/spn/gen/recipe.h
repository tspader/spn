#include "spn/spn.h"

#ifndef SPN_OPTIONS
  spn_gen_recipe_h__SPN_OPTIONS_was_not_defined_in_recipe spn_static_assert;
  #define SPN_OPTIONS()
#endif

#ifndef SPN_PACKAGE
  spn_gen_recipe_h__SPN_PACKAGE_was_not_defined_in_recipe spn_static_assert;
  #define SPN_PACKAGE undefined
#endif

#define _SPN_OPTIONS_T(PACKAGE) spn_##PACKAGE##_options_t
#define SPN_OPTIONS_T(PACKAGE) _SPN_OPTIONS_T(PACKAGE)
#define SPN_OPTION(TYPE, NAME) TYPE NAME;

typedef struct {
  spn_dep_kind_t kind;
  SPN_OPTIONS()
} SPN_OPTIONS_T(SPN_PACKAGE);


#undef SPN_RECIPE_FAILED
#undef SPN_PACKAGE
#undef SPN_OPTIONS
#undef SPN_OPTION
