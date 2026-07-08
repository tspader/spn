#include <cpptrace/cpptrace.hpp>

int main() {
  cpptrace::raw_trace trace = cpptrace::generate_raw_trace();
  return trace.frames.empty() ? 1 : 0;
}
