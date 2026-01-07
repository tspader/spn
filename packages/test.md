# SPN Test Packages

Ten semantic test packages for testing the spn build graph and dependency resolution.

## Dependency Graph

```
Layer 0:  spn_core    spn_simd
             |           |
Layer 1:  spn_alloc   spn_json
             |        /      \
Layer 2:  spn_log  spn_schema  spn_config
             |    /      |
Layer 3:  spn_net    spn_codegen
                  \      | (build dep)
Layer 4:          spn_orm
```

## Packages

| Package | Description | deps.package | deps.build |
|---------|-------------|--------------|------------|
| spn_core | base utilities | - | - |
| spn_simd | SIMD intrinsics | - | - |
| spn_alloc | memory allocator | spn_core | - |
| spn_json | JSON parser | spn_simd | - |
| spn_schema | schema validation | spn_json | - |
| spn_config | configuration | spn_json | - |
| spn_log | logging | spn_alloc | - |
| spn_codegen | code generation | spn_schema | - |
| spn_net | networking | spn_log, spn_json | - |
| spn_orm | ORM | spn_schema, spn_log | spn_codegen |

## Structure

Each package contains:
- `spn.toml` - package manifest
- `spn.c` - build script (copies header to include dir)
- `spn_<name>.h` - header-only library

## Testing

Test the full dependency graph with spn_orm:
```bash
cd packages/spn_orm
tspn graph -o graph.md
```

Test cross-dependencies with spn_net:
```bash
cd packages/spn_net
tspn graph -o graph.md
```
