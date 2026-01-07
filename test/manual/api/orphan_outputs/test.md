Tests that only orphan outputs (no consumers) connect to `sync::user`.

- `orphan_header.h` → `sync::user` (orphan)
- `consumed_header.h` → `consumer` node (NOT to sync::user)
- `consumer` stamp → `sync::user` (orphan stamp)
