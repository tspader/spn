#include <thread>
#include <string>

int main() {
  std::string s = "runtime";
  std::thread t([&]{ s += " threads"; });
  t.join();
  return s.size() == 15 ? 0 : 1;
}
