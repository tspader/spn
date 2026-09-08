#if defined(__x86_64__)
#define SYS_WRITE 1
#define SYS_EXIT 60

static long syscall3(long number, long x, long y, long z) {
  long result;
  __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(x), "S"(y), "d"(z) : "rcx", "r11", "memory");
  return result;
}

__asm__(
  ".globl _start\n"
  "_start:\n"
  "  xor %ebp, %ebp\n"
  "  and $-16, %rsp\n"
  "  call start\n"
);
#elif defined(__aarch64__)
#define SYS_WRITE 64
#define SYS_EXIT 93

static long syscall3(long number, long x, long y, long z) {
  register long x8 __asm__("x8") = number;
  register long x0 __asm__("x0") = x;
  register long x1 __asm__("x1") = y;
  register long x2 __asm__("x2") = z;
  __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory");
  return x0;
}

__asm__(
  ".globl _start\n"
  "_start:\n"
  "  mov x29, #0\n"
  "  bl start\n"
);
#endif

void start() {
  const char* line = "hello from nolibc\n";
  syscall3(SYS_WRITE, 1, (long)line, 18);
  syscall3(SYS_EXIT, 0, 0, 0);
}
