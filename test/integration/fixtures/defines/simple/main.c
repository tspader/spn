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

#if (defined(_MSC_VER) && !defined(__clang__)) != defined(SPN_BUILD_DRIVER_MSVC)
  #error SPN_BUILD_DRIVER_MSVC disagrees with _MSC_VER
#endif
#if (defined(__GNUC__) && !defined(__clang__)) != defined(SPN_BUILD_DRIVER_GCC)
  #error SPN_BUILD_DRIVER_GCC disagrees with __GNUC__
#endif
#if defined(__clang__) != (defined(SPN_BUILD_DRIVER_CLANG) || defined(SPN_BUILD_DRIVER_ZIG))
  #error SPN_BUILD_DRIVER_CLANG and SPN_BUILD_DRIVER_ZIG disagree with __clang__
#endif

#if defined(SPN_BUILD_DRIVER_GCC) != defined(GATE_GCC)
  #error GATE_GCC disagrees with SPN_BUILD_DRIVER_GCC
#endif
#if defined(SPN_BUILD_DRIVER_CLANG) != defined(GATE_CLANG)
  #error GATE_CLANG disagrees with SPN_BUILD_DRIVER_CLANG
#endif
#if defined(SPN_BUILD_DRIVER_MSVC) != defined(GATE_MSVC)
  #error GATE_MSVC disagrees with SPN_BUILD_DRIVER_MSVC
#endif
#if defined(SPN_BUILD_DRIVER_ZIG) != defined(GATE_ZIG)
  #error GATE_ZIG disagrees with SPN_BUILD_DRIVER_ZIG
#endif

#ifndef SPN_BUILD_MODE_DEBUG
  #error SPN_BUILD_MODE_DEBUG is not defined
#endif

int main(void) {
  return 0;
}
