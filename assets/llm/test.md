# note

We're porting our tests from a modified utest.h to a proper spiritual successor thereof. It's upstream in sp.h, and it's called sp_test.h. It's not compatible with utest.h, so we're temporarily going to have a split test setup.

Do not write new tests for utest.h. Do not update utest.h tests. You touch it, you port it. Unit tests only.

# overview

Tests must be written declaratively, by expressing test cases as pure data which are run through a test executor. The executor does setup, execution, expectation, and teardown according to the data in the test case. Imperative logic lives in the executor.
- You can (and should, for larger suites) have multiple executors. Testing a feature does not mean jamming every test into one executor.
- You can drop into imperative logic only when there is a single test which does not conform to the pattern

Tests should be:
- Minimal. Every test should test exactly one feature, and should only have what's needed for that. No fluff.
- Orthogonal. Every feature should be tested in exactly one place.
- High value. We shouldn't burden ourself with brittle tests, or tests that wouldn't catch real bugs.
- Complete. Every feature should be tested.

# running
Running the tests via CTest is easiest, and outputs to an ISO timestamped directory in `.tmp`:
```sh
make test
```
or, equivalently, `ctest --test-dir .build/work/$TRIPLE --output-on-failure`.

# notes

- `test/tools/action.h` has the ops for our integration test "bytecode"; the following ops are being phased out and are banned because they are either (a) hostile to testing cross compiles, or (b) fragile, fuzzy string matching against text that we never intended to be stable:
  - ACTION_VERIFY_FILE_CONTAINS
  - ACTION_VERIFY_FILE_NOT_CONTAINS
  - ACTION_VERIFY_CLI_CONTAINS
  - ACTION_VERIFY_CLI_NOT_CONTAINS
  - ACTION_RUN_BIN
  - ACTION_RUN_TEST
  - ACTION_VERIFY_DIR_COUNT
  - ACTION_VERIFY_EVENT_COUNT
- The resolver fuzzer only runs with SPN_FUZZ_ENABLE set; it currently finds known resolver bugs (greedy incompleteness), so it is not wired into CI. Don't treat its failures as regressions unless you changed the resolver.
- Use literal friendly types, like `const c8*` and `T [N]` (i.e. fixed size C arrays)
- Use `sp_carr_detect_len(arr, it, expr)` from `sp/macro.h` to detect a zero sentinel terminated C array; length increments until `!expr`
- Use a separate struct for `.expect`
- Never explicitly initialize fields which are zero initialized (e.g. do not set `.err = SP_OK`)
- When test cases need multistep, ordered setup, used a tagged union of actions (see: `fs_setup_t`)
- One class of tests per C file. If a suite has multiple, write the individual C files in `test/$module/`, and then have `test/module.c` `#include` all the C files (see: `test/fs.c`)
- Always use single capital letters as IDs and content when needed. For example, don't name a test package "mathlib"; name it "A".

# example

Follow this structure when adding new tests.

```c
#include "spn_test.h"

#define FOO_TEST_MAX_BAZ 8

typedef struct {
  bool spum;
  sp_err_t err;
  const c8* kram;
} expect_t;

typedef struct {
  u32 bar;
  const c8* baz [FOO_TEST_MAX_BAZ]
  foo_expect_t expect;
} test_t;

static test_t tests [] = {
  {
    .bar = 1,
    .baz = { "A", "B", "C" }
    .expect = {
      .kram = "A"
      // Note that we didn't specify fields covered by zero initialization
    }
  },
  // ...
}

sp_test_each(suite, executor_name, test_t, tests) {
  foo_t foo = qux(it->bar);

  u32 len = 0;
  sp_carr_detect_len(it->baz, len, it->baz[len]);
  sp_must_eq(t, len, sp_da_size(foo.baz));
  sp_for(len, i) {
    sp_expect_str_eq_c(t, foo.baz[i], it->baz[i]);
  }
  // ...
}
```
