#if defined(__x86_64__)
#define SYS_EXIT 60

static long syscall1(long number, long x) {
  long result;
  __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(x) : "rcx", "r11", "memory");
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
#define SYS_EXIT 93

static long syscall1(long number, long x) {
  register long x8 __asm__("x8") = number;
  register long x0 __asm__("x0") = x;
  __asm__ volatile ("svc #0" : "+r"(x0) : "r"(x8) : "memory");
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
  syscall1(SYS_EXIT, 0);
}
