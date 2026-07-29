// The sp and spit implementations live alone in this TU; this binary is a
// single TU, so it doubles as its own spit_main.c
#define SP_IMPLEMENTATION
#include "sp.h"

#include "spn_test.h"

#include "filter/filter.h"

s32 main(s32 argc, const c8** argv) {
  return sp_test_main(argc, argv, SP_NULLPTR);
}

typedef struct {
  bool pass;
} expect_t;

typedef struct {
  const c8* name;
  spn_target_selection_kind_t selection;
  spn_target_kind_t target_kind;
  spn_target_kind_t selected_kind;
  spn_target_rule_kind_t selected_rule;
  const c8* target_name;
  const c8* requested_name;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { "default_selects_lib",    SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_LIB,                   SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "spum", SP_NULLPTR, { true } },
  { "default_selects_exe",    SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_EXE,                   SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "main", SP_NULLPTR, { true } },
  { "default_selects_test",   SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_TEST,                  SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "test", SP_NULLPTR, { true } },
  { "default_skips_script",   SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_SCRIPT,                SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "tool", SP_NULLPTR, { false } },
  { "default_skips_configure", SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_CONFIGURE_METAPROGRAM, SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "configure", SP_NULLPTR, { false } },
  { "default_skips_build",    SPN_TARGET_SELECTION_DEFAULT, SPN_TARGET_BUILD_METAPROGRAM,     SPN_TARGET_LIB, SPN_TARGET_RULE_NONE, "build", SP_NULLPTR, { false } },

  { "lib_rule_none_skips",    SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_NONE,  "spum", SP_NULLPTR, { false } },
  { "lib_rule_all_selects",   SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_ALL,   "spum", SP_NULLPTR, { true } },
  { "lib_named_match",        SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_NAMED, "spum", "spum", { true } },
  { "lib_named_miss",         SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB, SPN_TARGET_LIB, SPN_TARGET_RULE_NAMED, "spum", "spam", { false } },

  { "exe_rule_none_skips",    SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_NONE,  "main", SP_NULLPTR, { false } },
  { "exe_rule_all_selects",   SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_ALL,   "main", SP_NULLPTR, { true } },
  { "exe_named_match",        SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_NAMED, "main", "main", { true } },
  { "exe_named_miss",         SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE, SPN_TARGET_EXE, SPN_TARGET_RULE_NAMED, "main", "other", { false } },

  { "test_rule_none_skips",   SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_NONE,  "test", SP_NULLPTR, { false } },
  { "test_rule_all_selects",  SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_ALL,   "test", SP_NULLPTR, { true } },
  { "test_named_match",       SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_NAMED, "test", "test", { true } },
  { "test_named_miss",        SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST, SPN_TARGET_TEST, SPN_TARGET_RULE_NAMED, "test", "other", { false } },

  { "script_rule_none_skips",  SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_NONE,  "tool", SP_NULLPTR, { false } },
  { "script_rule_all_selects", SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_ALL,   "tool", SP_NULLPTR, { true } },
  { "script_named_match",      SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_NAMED, "tool", "tool", { true } },
  { "script_named_miss",       SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_SCRIPT, SPN_TARGET_RULE_NAMED, "tool", "other", { false } },

  { "lib_rule_skips_exe",     SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_EXE,    SPN_TARGET_LIB,    SPN_TARGET_RULE_ALL, "main", SP_NULLPTR, { false } },
  { "exe_rule_skips_test",    SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_TEST,   SPN_TARGET_EXE,    SPN_TARGET_RULE_ALL, "test", SP_NULLPTR, { false } },
  { "test_rule_skips_script", SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_SCRIPT, SPN_TARGET_TEST,   SPN_TARGET_RULE_ALL, "tool", SP_NULLPTR, { false } },
  { "script_rule_skips_lib",  SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_LIB,    SPN_TARGET_SCRIPT, SPN_TARGET_RULE_ALL, "spum", SP_NULLPTR, { false } },

  { "all_rule_skips_configure", SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_CONFIGURE_METAPROGRAM, SPN_TARGET_LIB, SPN_TARGET_RULE_ALL, "configure", SP_NULLPTR, { false } },
  { "all_rule_skips_build",     SPN_TARGET_SELECTION_EXPLICIT, SPN_TARGET_BUILD_METAPROGRAM,     SPN_TARGET_LIB, SPN_TARGET_RULE_ALL, "build", SP_NULLPTR, { false } },
};

static spn_target_rule_t* target_rule(spn_target_selection_t* selection, spn_target_kind_t kind) {
  switch (kind) {
    case SPN_TARGET_LIB: return &selection->lib;
    case SPN_TARGET_EXE: return &selection->bin;
    case SPN_TARGET_TEST: return &selection->test;
    case SPN_TARGET_SCRIPT: return &selection->script;
    case SPN_TARGET_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_BUILD_METAPROGRAM: break;
  }
  sp_unreachable_return(SP_NULLPTR);
}

sp_test_each(filter, target_selection, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_target_selection_t selection = { .kind = it->selection };

  if (it->selection == SPN_TARGET_SELECTION_EXPLICIT) {
    spn_target_rule_t* rule = target_rule(&selection, it->selected_kind);
    rule->kind = it->selected_rule;
    if (it->requested_name) {
      rule->names = sp_da_new(mem, sp_str_t);
      sp_da_push(rule->names, sp_str_view(it->requested_name));
    }
  }

  spn_target_info_t target = {
    .name = sp_str_view(it->target_name),
    .kind = it->target_kind,
  };
  sp_expect_eq(t, it->expect.pass, spn_target_selection_pass(&selection, &target));

  return SP_OK;
}
