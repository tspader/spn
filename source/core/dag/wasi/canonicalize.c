#include "dag/wasi/canonicalize.h"

static bool seg_is_relative(sp_str_t seg) {
  return sp_str_equal(seg, sp_str_lit(".")) || sp_str_equal(seg, sp_str_lit(".."));
}

sp_str_t spn_dag_wasi_canonicalize(sp_mem_t mem, sp_str_t path) {
  sp_str_t canonical = sp_fs_canonicalize_path(mem, path);
  if (!sp_str_empty(canonical)) {
    return canonical;
  }

  sp_da(sp_str_t) tail = sp_da_new(mem, sp_str_t);
  sp_str_t prefix = sp_fs_trim_path(path);
  sp_str_t canon = sp_zero;
  while (sp_str_empty(canon)) {
    sp_str_t parent = sp_fs_parent_path(prefix);
    if (sp_str_empty(parent) && sp_fs_is_absolute(prefix)) {
      parent = sp_str_lit("/");
    }
    if (parent.len == 2 && parent.data[1] == ':') {
      parent = sp_str_concat(mem, parent, sp_str_lit("/"));
    }
    if (sp_str_empty(parent) || parent.len >= prefix.len) {
      return sp_str_lit("");
    }
    sp_da_push(tail, sp_fs_get_name(prefix));
    canon = sp_fs_canonicalize_path(mem, parent);
    prefix = sp_fs_trim_path(parent);
  }

  while (!sp_da_empty(tail)) {
    sp_str_t seg = *sp_da_back(tail);
    sp_da_pop(tail);
    sp_str_t candidate = sp_fs_join_path(mem, canon, seg);
    sp_str_t next = sp_fs_canonicalize_path(mem, candidate);
    if (sp_str_empty(next)) {
      return seg_is_relative(seg) ? canon : candidate;
    }
    canon = next;
  }

  return canon;
}
