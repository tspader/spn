#include "core/core.h"

sp_str_t spn_tree_root(spn_tree_roots_t roots, spn_tree_t tree) {
  return tree == SPN_TREE_MANIFEST ? roots.recipe : roots.source;
}

sp_str_t spn_tree_path_resolve(sp_mem_t mem, spn_tree_roots_t roots, spn_tree_path_t entry) {
  if (sp_fs_is_absolute(entry.path)) {
    return entry.path;
  }
  return sp_fs_join_path(mem, spn_tree_root(roots, entry.tree), entry.path);
}

void spn_wake_ring(spn_wake_t* wake) {
  if (!wake->fn) {
    return;
  }
  if (sp_atomic_u32_cas(&wake->signaled, 0, 1, SP_ATOMIC_SEQ_CST)) {
    wake->fn(wake->data);
  }
}

void spn_wake_pulse(spn_wake_t* wake) {
  if (!wake->fn) {
    return;
  }
  sp_atomic_u32_store(&wake->signaled, 1, SP_ATOMIC_SEQ_CST);
  wake->fn(wake->data);
}

void spn_wake_rearm(spn_wake_t* wake) {
  sp_atomic_u32_store(&wake->signaled, 0, SP_ATOMIC_SEQ_CST);
}
