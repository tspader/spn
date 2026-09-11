static void halt() {
#if defined(__x86_64__)
  __asm__ volatile ("hlt");
#elif defined(__aarch64__)
  __asm__ volatile ("wfi");
#endif
}

void kmain() {
  for (;;) {
    halt();
  }
}
