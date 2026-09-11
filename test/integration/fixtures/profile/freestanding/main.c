int a();

void _start() {
  volatile int x = a();
  volatile int* p = &x;
  x = -x;
  p = p + x;
  for (;;) {
  }
}
