Tests stamp node with file input but no output.

- `gen_data` produces `data.txt`
- `validate` consumes `data.txt` but has no output (stamp-only)
- `finalize` links to `validate`, produces header

Verifies mixed file-input + stamp-output nodes.
