#ifndef SPN_BUILD
  #error SPN_BUILD is not defined
#endif

#if defined(_WIN32) != defined(SPN_BUILD_OS_WINDOWS)
  #error SPN_BUILD_OS_WINDOWS disagrees with _WIN32
#endif
#if defined(__APPLE__) != defined(SPN_BUILD_OS_MACOS)
  #error SPN_BUILD_OS_MACOS disagrees with __APPLE__
#endif
#if defined(__linux__) != defined(SPN_BUILD_OS_LINUX)
  #error SPN_BUILD_OS_LINUX disagrees with __linux__
#endif
#if defined(__wasi__) != defined(SPN_BUILD_OS_WASI)
  #error SPN_BUILD_OS_WASI disagrees with __wasi__
#endif

#if (defined(__x86_64__) || defined(_M_X64)) != defined(SPN_BUILD_ARCH_X86_64)
  #error SPN_BUILD_ARCH_X86_64 disagrees with the compiler
#endif
#if (defined(__aarch64__) || defined(_M_ARM64)) != defined(SPN_BUILD_ARCH_AARCH64)
  #error SPN_BUILD_ARCH_AARCH64 disagrees with the compiler
#endif
#if defined(__wasm32__) != defined(SPN_BUILD_ARCH_WASM32)
  #error SPN_BUILD_ARCH_WASM32 disagrees with the compiler
#endif

#ifndef SPN_BUILD_MODE_DEBUG
  #error SPN_BUILD_MODE_DEBUG is not defined
#endif

int main(void) {
  return 0;
}
