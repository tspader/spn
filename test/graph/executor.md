# Threaded Task Queue for Build Graph

## Overview

Add a parallel executor that runs dirty commands using a thread pool. Commands become ready when all their inputs are available (either global inputs or outputs from completed commands).

## Key SP APIs

```c
// Ring buffer
void sp_rb_init(sp_rb_t* buffer, u32 capacity, u32 element_size);
void* sp_rb_push(sp_rb_t* buffer, void* data);
void* sp_rb_pop(sp_rb_t* buffer);
bool sp_rb_is_empty(sp_rb_t* buffer);

// Threading
void sp_thread_init(sp_thread_t* thread, sp_thread_fn_t fn, void* userdata);
void sp_thread_join(sp_thread_t* thread);
void sp_mutex_init(sp_mutex_t* mutex, sp_mutex_kind_t kind);
void sp_mutex_lock(sp_mutex_t* mutex);
void sp_mutex_unlock(sp_mutex_t* mutex);
void sp_semaphore_init(sp_semaphore_t* semaphore);
void sp_semaphore_wait(sp_semaphore_t* semaphore);
void sp_semaphore_signal(sp_semaphore_t* semaphore);
```

## Structs

```c
typedef struct {
  spn_bg_id_t cmd_id;
  sp_ps_output_t output;
} spn_bg_exec_error_t;

typedef struct {
  sp_rb(spn_bg_id_t) ready_queue;  // Commands ready to execute
  sp_mutex_t mutex;                 // Guards all mutable state
  sp_semaphore_t work_available;    // Signaled when work added or shutdown

  sp_ht(spn_bg_id_t, bool) completed;  // Commands that have finished
  sp_ht(spn_bg_id_t, bool) dirty_cmds; // Commands that need to run

  spn_build_graph_t* graph;
  u32 num_threads;
  sp_da(sp_thread_t) threads;
  sp_da(spn_bg_id_t) ran;              // Commands that actually ran
  sp_da(spn_bg_exec_error_t) errors;   // Failed commands

  sp_atomic_s32 active_workers;        // Workers currently executing a command
  sp_atomic_s32 shutdown;              // Signal workers to exit
} spn_bg_executor_t;

typedef struct {
  spn_bg_executor_t* executor;
  u32 thread_id;
} spn_bg_worker_ctx_t;
```

## API

```c
spn_bg_executor_t* spn_bg_executor_new(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, u32 num_threads);
void spn_bg_executor_run(spn_bg_executor_t* executor);
void spn_bg_executor_destroy(spn_bg_executor_t* executor);
```

## Algorithm

### Initialization
1. Create executor, init mutex/semaphore
2. Copy dirty commands from `spn_bg_dirty_t` into executor
3. For each dirty command, check if it's immediately ready (all inputs available)
4. Push ready commands to `ready_queue`, signal semaphore for each
5. Spawn worker threads

### Command Readiness Check
```c
bool spn_bg_cmd_is_ready(spn_bg_executor_t* ex, spn_bg_id_t cmd_id) {
  spn_build_cmd_t* cmd = spn_bg_find_command(ex->graph, cmd_id);

  sp_da_for(cmd->consumes, i) {
    spn_build_file_t* file = spn_bg_find_file(ex->graph, cmd->consumes[i]);

    // Global input - always available
    if (!file->producer.occupied) continue;

    // Produced file - check if producer completed
    if (!sp_ht_key_exists(ex->completed, file->producer)) {
      return false;
    }
  }
  return true;
}
```

### Worker Thread Loop (high-level)
```
worker_fn:
  loop:
    wait on semaphore

    if shutdown flag set:
      signal semaphore (cascade to next worker)
      return

    lock mutex
    if queue empty:
      unlock mutex
      continue  // spurious wake
    pop cmd from ready_queue
    active_workers++
    unlock mutex

    run the command (sp_ps_run)

    lock mutex
    mark cmd completed, record in ran list

    if command failed:
      record error
      // don't push downstream - stop propagation
    else:
      for each downstream dirty command:
        if now ready: push to queue, signal semaphore

    active_workers--

    // Check termination: queue empty AND no other workers active
    if queue empty AND active_workers == 0:
      set shutdown flag
      signal semaphore (starts cascade)

    unlock mutex
```

### Main Run Function
```
executor_run:
  join all threads
  // Workers self-terminate when work exhausted
```

## Testing Strategy

Tests verify that exactly the dirty commands run, regardless of order:

```c
UTEST_F(spn_executor_tests, runs_only_dirty_commands) {
  // Create graph with files on disk
  // Make some files out of date
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);

  spn_bg_executor_t* ex = spn_bg_executor_new(graph, dirty, 4);
  spn_bg_executor_run(ex);

  // Verify: exactly the dirty commands ran
  EXPECT_EQ(sp_da_size(ex->ran), expected_dirty_count);

  // Each dirty command should be in ran exactly once
  sp_da_for(ex->ran, i) {
    EXPECT_TRUE(sp_ht_key_exists(dirty->commands, ex->ran[i]));
  }

  // Each dirty command should have ran
  sp_ht_for(dirty->commands, it) {
    spn_bg_id_t* cmd_id = sp_ht_it_getkp(dirty->commands, it);
    bool found = false;
    sp_da_for(ex->ran, i) {
      if (ex->ran[i].index == cmd_id->index) found = true;
    }
    EXPECT_TRUE(found);
  }
}
```

Test cases:
1. **Single dirty command** - verifies basic execution
2. **Chain of dirty commands** - verifies dependency ordering works
3. **Independent dirty commands** - verifies parallel execution doesn't break anything
4. **Mixed clean/dirty** - verifies clean commands don't run
5. **All clean** - verifies nothing runs when nothing is dirty
6. **Command failure** - verifies downstream commands don't run, error is recorded

## Files to Modify

- `graph.h`: Add executor structs and API declarations, implementation
