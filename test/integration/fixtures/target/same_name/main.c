#if defined(LIB)
int a() {
  return 69;
}
#else
int a();

int main() {
  return a() != 69;
}
#endif
