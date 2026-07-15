int main() {
#ifdef RELEASE_OS
  return 20;
#else
  return 10;
#endif
}
