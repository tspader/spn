int main() {
#if defined(FACT_OS) && defined(FACT_ARCH) && defined(FACT_MODE) && !defined(FACT_OTHER_OS)
  return 0;
#else
  return 1;
#endif
}
