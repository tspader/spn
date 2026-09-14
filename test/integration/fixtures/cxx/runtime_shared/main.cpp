#include <string>

int main() {
  std::string s = "runtime";
  s += " shared";
  return s.size() == 14 ? 0 : 1;
}
