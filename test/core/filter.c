#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "filter/filter.h"

UTEST_MAIN()

typedef struct {
  spn_target_selection_kind_t selection;
  spn_target_kind_t target_kind;
  spn_target_kind_t selected_kind;
  spn_target_rule_kind_t selected_rule;
  const c8* target_name;
  const c8* requested_name;
  bool expected;
} target_selection_test_t;

static spn_target_rule_t* target_rule(spn_target_selection_t* selection, spn_target_kind_t kind) {
  switch (kind) {
    case SPN_TARGET_LIB: return &selection->targets.lib;
    case SPN_TARGET_EXE: return &selection->targets.bin;
    case SPN_TARGET_TEST: return &selection->targets.test;
    case SPN_TARGET_SCRIPT: return &selection->targets.script;
    case SPN_TARGET_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_BUILD_METAPROGRAM: break;
  }
  sp_unreachable_return(SP_NULLPTR);
}

static void run_target_selection_test(s32* utest_result, target_selection_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_target_selection_t selection = { .kind = test.selection };

  if (test.selection == SPN_TARGET_SELECTION_EXPLICIT) {
    spn_target_rule_t* rule = target_rule(&selection, test.selected_kind);
    rule->kind = test.selected_rule;
    if (test.requested_name) {
      rule->names = sp_da_new(scratch.mem, sp_str_t);
      sp_da_push(rule->names, sp_str_view(test.requested_name));
    }
  }

  spn_target_info_t target = {
    .name = sp_str_view(test.target_name),
    .kind = test.target_kind,
  };
  EXPECT_EQ(test.expected, spn_target_selection_pass(&selection, &target));
  sp_mem_end_scratch(scratch);
}

UTEST(filter, target_selection) {
  target_selection_test_t tests [] = {
    { SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_LIB,                   SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "spum", SP_NULLPTR, true  },
    { SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_EXE,                   SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "main", SP_NULLPTR, true  },
    { SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_TEST,                  SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "test", SP_NULLPTR, true  },
    { SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_SCRIPT,                SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "tool", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_CONFIGURE_METAPROGRAM, SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "configure", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_BUILD_METAPROGRAM,     SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "build", SP_NULLPTR, false },

    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_NONE,  "spum", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_ALL,   "spum", SP_NULLPTR, true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_NAMED, "spum", "spum", true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_NAMED, "spum", "spam", false },

    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_NONE,  "main", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_ALL,   "main", SP_NULLPTR, true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_NAMED, "main", "main", true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_NAMED, "main", "other", false },

    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_NONE,  "test", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_ALL,   "test", SP_NULLPTR, true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_NAMED, "test", "test", true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_NAMED, "test", "other", false },

    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_NONE,  "tool", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_ALL,   "tool", SP_NULLPTR, true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_NAMED, "tool", "tool", true  },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_NAMED, "tool", "other", false },

    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE,    SPN_TARGET_LIB,    SPN_TARGET_RULE_ALL, "main", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST,   SPN_TARGET_EXE,    SPN_TARGET_RULE_ALL, "test", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_TEST,   SPN_TARGET_RULE_ALL, "tool", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB,    SPN_TARGET_SCRIPT, SPN_TARGET_RULE_ALL, "spum", SP_NULLPTR, false },

    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_CONFIGURE_METAPROGRAM, SPN_TARGET_LIB, SPN_TARGET_RULE_ALL, "configure", SP_NULLPTR, false },
    { SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_BUILD_METAPROGRAM,     SPN_TARGET_LIB, SPN_TARGET_RULE_ALL, "build", SP_NULLPTR, false },
  };

  sp_carr_for(tests, it) {
    run_target_selection_test(utest_result, tests[it]);
  }
}
