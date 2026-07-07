int main() {
#if defined(NOT_MSVC) && !defined(NOT_LINUX)
  return 0;
#else
  return 1;
#endif
}
