Tests diamond dependency pattern: A→B, A→C, B→D, C→D.

- `base` produces `base.h`
- `left` and `right` both consume `base.h`
- `final` consumes both `left.h` and `right.h`
