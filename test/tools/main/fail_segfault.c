#include "utest.h"

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

UTEST(stream, pass) {
  EXPECT_EQ(69, 69);
}

UTEST(stream, segfault) {
  fprintf(stdout, "hello, stdout!\n");
  fflush(stdout);

  int* foo = 0;
  *foo = 69;

  EXPECT_EQ(69, 420);
}
