#ifndef VIA_FLAG
#error "target flags did not reach this compile"
#endif

int extra2_value(void) {
  return VIA_FLAG;
}
