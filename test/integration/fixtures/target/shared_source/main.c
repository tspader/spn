#if SPUM == 69
int spum() {
  return SPUM;
}
#else
int spum();

int main() {
  return spum() != 69;
}
#endif
