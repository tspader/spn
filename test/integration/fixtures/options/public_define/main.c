#include "ui.h"

int main() {
#ifdef UI_FREETYPE
  return ui_value() == 1 ? 0 : 1;
#else
  return 1;
#endif
}
