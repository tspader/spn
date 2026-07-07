void _ZSt9terminatev(void);

int main(int num_args, const char** args) {
  if (num_args > 100) {
    _ZSt9terminatev();
  }
  return 0;
}
