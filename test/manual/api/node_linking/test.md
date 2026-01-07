Tests `spn_node_link` for explicit ordering without shared files.

- `setup` has no outputs (uses auto-generated stamp)
- `codegen` links to `setup` via `spn_node_link`

Verifies stamp-based ordering for nodes without file outputs.
