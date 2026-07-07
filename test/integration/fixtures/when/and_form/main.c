int main() {
#ifdef RELEASE_LINUX
  return 20;
#else
  return 10;
#endif
}
