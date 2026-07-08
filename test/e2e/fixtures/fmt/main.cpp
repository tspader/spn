#include <fmt/format.h>

int main() {
  return fmt::format("{}", 42) == "42" ? 0 : 1;
}
