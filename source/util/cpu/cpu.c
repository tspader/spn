#if defined(__linux__) && !defined(_GNU_SOURCE)
  #define _GNU_SOURCE
#endif

#include "cpu/cpu.h"

#if defined(SP_LINUX)
  #include <sched.h>
  #include <unistd.h>

u32 spn_cpu_count(void) {
  cpu_set_t set;
  if (!sched_getaffinity(0, sizeof(set), &set)) {
    u32 count = (u32)CPU_COUNT(&set);
    if (count) {
      return count;
    }
  }
  long online = sysconf(_SC_NPROCESSORS_ONLN);
  return online > 0 ? (u32)online : 1;
}

#elif defined(SP_MACOS)
  #include <sys/sysctl.h>

u32 spn_cpu_count(void) {
  s32 count = 0;
  size_t len = sizeof(count);
  if (!sysctlbyname("hw.activecpu", &count, &len, SP_NULLPTR, 0) && count > 0) {
    return (u32)count;
  }
  return 1;
}

#elif defined(SP_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>

u32 spn_cpu_count(void) {
  DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  return count ? (u32)count : 1;
}

#else
  #error "spn_cpu_count: unsupported platform"
#endif
