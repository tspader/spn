Tests implicit dependency via shared file.

- `phase1` produces `intermediate.h`
- `phase2` consumes `intermediate.h`, produces `final.h`

Verifies file node deduplication when the same path is used as output and input.
