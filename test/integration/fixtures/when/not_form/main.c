int main() {
#if defined(NOT_MSVC) && !defined(NOT_DEBUG)
  return 0;
#else
  return 1;
#endif
}
