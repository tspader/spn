#if GATE_WASM32 != 69
#error GATE_WASM32
#endif

#if defined(GATE_X86_64) || defined(GATE_AARCH64)
#error GATE_EXCLUSION
#endif

#if defined(SPUM_FLAG)
#error SPUM_FLAG
#endif

int wiring() {
  return 69;
}
