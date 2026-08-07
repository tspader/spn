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

- `test/integration/tools/harness.h` has the ops for our integration test "bytecode"; the following ops are being phased out and are banned because they are either (a) hostile to testing cross compiles, or (b) fragile, fuzzy string matching against text that we never intended to be stable:
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

# integration

The integration port to sp_test is a straight port: the action bytecode and the small executor set stay, and banned ops carry over as-is except where a disposition below says otherwise.

Assertion dispositions:
- Error text matched via CLI_CONTAINS migrates to the structured error (err kind / events), not the rendered message.
- Assertions on informational CLI output are low value. Delete the assertion; delete the test if that's all it verified. Literal CLI text tests are pointless.
- log.c is deleted except user_log.shown_on_failure and user_log.hidden_normally: the structured error should carry the subprocess output, and those two tests assert that routing.
- Manifest/lock content checks (cli add/update): load the manifest through the real TU. If the code is misfactored such that the load TU can't be linked in, raw text verification is acceptable.
- Build-script witness files: exact file equality, never contains.
- compile_commands checks: VERIFY_CC_ARG only, never string matching.

The script suite is rewritten from first principles into five case tables, one per feature class:
- Node graph structure (basic node, chains, fan-in, multi-output, orphan outputs): build-only. Fixtures self-verify with `_Static_assert` on generated headers, so the expectation is just build rc + artifacts exist. Never run the binary.
- Programmatic configuration APIs (`spn_add_*`, handle misuse): same executor, simple and build-only. `embed` is the single runtime exception; embedded bytes can only be checked by running.
- Node rerun: mutate inputs, then assert witness/output files by exact file equality.
- Build dep identity: an executor with first-class expects — locked packages, store entries per package (distinct build identities), configure runs per package (cache hits), absent paths. Each case asserts exactly one identity property; never stack them.
- Configure source selection and script errors: cases are {fixture, args, err}.

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
