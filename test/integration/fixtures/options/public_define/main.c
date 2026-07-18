#include "ui.h"

#ifndef UI_FREETYPE
#error "expected UI_FREETYPE"
#endif

int main() {
  return ui_value() == 1 ? 0 : 1;
}
