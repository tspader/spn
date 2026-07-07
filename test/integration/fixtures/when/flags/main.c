int main() {
#ifdef FLAG_LINUX
  return 0;
#else
  return 1;
#endif
}
