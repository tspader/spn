# User-Defined Build Graph Node Tests

## Test Matrix

| Test | APIs Covered | Pattern |
|------|-------------|---------|
| basic_node | `spn_add_node`, `spn_node_set_fn`, `spn_node_add_output`, `spn_write_file` | Single node with file output |
| chained_nodes | `spn_node_add_input` | Shared file creates implicit dependency |
| node_linking | `spn_node_link` | Explicit ordering without shared files; stamp node |
| stamp_chain | `spn_node_link` (chain) | Multiple stamp-only nodes chained via link() |
| stamp_input | `spn_node_add_input` (stamp) | Stamp node with file input but no output |
| user_data | `spn_node_set_user_data` | Passing config to callback |
| diamond_deps | `spn_node_add_input/output` | Diamond: A→B, A→C, B→D, C→D |
| fan_in | `spn_node_add_input` (multiple) | 4 nodes feed 1 combiner |
| multi_output | `spn_node_add_output` (multiple) | 1 node produces 3 files |
| cross_package | `spn_get_dep`, `spn_get_dir` | Access dependency context |

## Running Tests

```sh
cd test/manual/graph/<test_name>
rm -rf build && tspn build -p debug
./build/debug/store/bin/<test_name>
```

## Known Bugs Found

### BUG 1: Stamp files never written to disk
User nodes without explicit outputs get stamp paths computed at graph construction:
```c
sp_str_t stamp = sp_format("{}/{}.stamp", ..., node->tag);
```
But these files are never actually created. The graph executor marks commands complete in-memory but doesn't touch the stamp file on disk.

**Impact**: Stamp-only nodes are always "dirty" (missing_output=true). On clean build, ordering works due to in-memory tracking. After recompile of spn.c, dirtiness is recomputed from disk and stamp nodes are correctly marked dirty, but then build order is wrong.

**Reproduce**:
```sh
cd stamp_chain
rm -rf build && tspn build -p debug  # works
touch spn.c && tspn build -p debug   # fails: compile runs before fn nodes
```

### BUG 2: Build order after script recompile
When spn.c is modified, the build script is recompiled. Then compilation of the main binary runs before user fn nodes, even when the binary depends on files produced by those nodes.

**Cause**: Likely the graph isn't being reconstructed after script recompile, or the compilation target isn't properly wired to depend on user node outputs.
