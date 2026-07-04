static_assert(__cplusplus == 201402L, "cxx.standard was not applied");

struct base {
  virtual ~base() {}
  virtual int value() = 0;
};

struct derived : base {
  int value() override { return 69; }
};

int main(int num_args, const char** args) {
  (void)num_args;
  (void)args;

  base* b = new derived();
  int v = b->value();
  delete b;
  return v == 69 ? 0 : 1;
}
