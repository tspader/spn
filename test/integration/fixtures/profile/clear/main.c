#if defined(__SANITIZE_ADDRESS__)
  #define HAVE_ASAN 1
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer)
    #define HAVE_ASAN 1
  #endif
#endif

#if defined(WANT_ASAN) && !defined(HAVE_ASAN)
#error WANT_ASAN
#endif

#if !defined(WANT_ASAN) && defined(HAVE_ASAN)
#error HAVE_ASAN
#endif

int main() {
  return 0;
}
