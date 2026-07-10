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

- The fuzz tests currently fail. Don't run them.
- Use literal friendly types, like `const c8*` and `T [N]` (i.e. fixed size C arrays)
- Use `sp_carr_for()` + zero-as-sentinel (when possible) to avoid typing sentinels or lengths at the test site
- Use a separate struct for `.expect`
- Never explicitly initialize fields which are zero initialized (e.g. do not set `.err = SP_OK`)
- When test cases need multistep, ordered setup, used a tagged union of actions (see: `fs_setup_t`)
- One class of tests per C file. If a suite has multiple, write the individual C files in `test/$module/`, and then have `test/module.c` `#include` all the C files (see: `test/fs.c`)

# example

Follow this structure when adding new tests.

```c
#define FOO_TEST_MAX_BAZ 8

typedef struct {
  bool spum;
  sp_err_t err;
  const c8* kram;
} foo_expect_t;

typedef struct {
  u32 bar;
  const c8* baz [FOO_TEST_MAX_BAZ]
  foo_expect_t expect;
} foo_test_t;

UTEST_EMPTY_FIXTURE(foo)

void run_foo_test(s32* utest_result, foo_test_t t) {
  sp_carr_for(it, t.baz) {
    if (!t.baz[it]) break;
    // ...do something with baz[it]
  }

  EXPECT_TRUE(t.spum);
  // ...verify expectations
}

UTEST_F(foo, large_bar_ok) {
  run_foo_test(&ur, (foo_test_t) {
    .bar = 69,
    .baz = { "skam", "grum", "qux" },
    .expect = {
      .spum = 69
    }
  });
}
```
