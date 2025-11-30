# Build Graph Integration

## Goal
Replace ad-hoc thread-per-dep orchestration with graph executor.

## Graph Structure

```
[dep build cmd] --> [stamp file] --> [bin build cmd] --> [binary]
```

## Implementation

### 1. Move `test/graph/graph.h` to `source/graph.h`

### 2. Comment out TUI code in `spn_cli_build`

### 3. Add graph construction in `spn_cli_build`:

```c
spn_build_graph_t* graph = spn_bg_new();

// For each dep: [build cmd] -> [stamp]
sp_ht_for(app.deps, it) {
  spn_pkg_build_t* dep = sp_ht_it_getp(app.deps, it);

  spn_bg_id_t stamp = spn_bg_add_file(graph, dep->paths.stamp);
  spn_bg_id_t cmd = spn_bg_add_command(graph, SPN_BUILD_CMD_FN);
  spn_build_cmd_t* cmd_ptr = spn_bg_find_command(graph, cmd);
  cmd_ptr->fn.on_execute = spn_dep_build_execute;
  cmd_ptr->fn.user_data = dep;
  spn_build_command_add_output(graph, cmd, stamp);
}

// For each binary: [all dep stamps] -> [build cmd] -> [binary]
sp_ht_for(app.package.bin, it) {
  spn_bin_t* bin = sp_ht_it_getp(app.package.bin, it);
  sp_str_t binary_path = spn_pkg_build_get_bin_path(&app.build, bin);

  spn_bg_id_t binary = spn_bg_add_file(graph, binary_path);
  spn_bg_id_t cmd = spn_bg_add_command(graph, SPN_BUILD_CMD_FN);
  spn_build_cmd_t* cmd_ptr = spn_bg_find_command(graph, cmd);
  cmd_ptr->fn.on_execute = spn_bin_build_execute;
  cmd_ptr->fn.user_data = bin;

  // Each dep stamp is input to bin build
  sp_da_for(graph->files, n) {
    spn_bg_id_t stamp_id = graph->files[n].id;
    spn_build_command_add_input(graph, cmd, stamp_id);
  }
  spn_build_command_add_output(graph, cmd, binary);
}
```

### 4. Add execute callbacks

```c
void spn_dep_build_execute(spn_build_cmd_t* cmd, void* user_data) {
  spn_pkg_build_t* dep = (spn_pkg_build_t*)user_data;
  spn_pkg_build_run(dep);
}

void spn_bin_build_execute(spn_build_cmd_t* cmd, void* user_data) {
  spn_bin_t* bin = (spn_bin_t*)user_data;
  spn_dep_context_build_binary(&app.build, *bin);
}
```

### 5. Run executor

```c
spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);
spn_bg_executor_t* ex = spn_bg_executor_new(graph, dirty, num_threads);
spn_bg_executor_run(ex);
spn_bg_executor_join(ex);
```

## Files to Modify

1. `test/graph/graph.h` → `source/graph.h`
2. `source/spn.c`: include graph.h, add callbacks, replace build loop with graph construction + executor
