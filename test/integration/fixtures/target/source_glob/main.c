int pre();
int post();

int main(int num_args, const char** args) {
  return pre() + post() == 69 ? 0 : 1;
}
