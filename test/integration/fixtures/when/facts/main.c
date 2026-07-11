int main() {
#if defined(FACT_OS) && defined(FACT_ARCH) && defined(FACT_MODE) && defined(FACT_OPT) && defined(FACT_NO_TSAN) && !defined(FACT_OTHER_OS) && !defined(FACT_SANITIZED)
  return 0;
#else
  return 1;
#endif
}
