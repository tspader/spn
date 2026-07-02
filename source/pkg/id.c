#include "intern/types.h"

#include "intern/intern.h"
#include "pkg/id.h"
#include "sp/macro.h"

sp_intern_str_t spn_pkg_canonicalize_name(sp_str_t name) {
  if (sp_str_empty(name)) return sp_zero_struct(sp_str_t);
  spn_pkg_id_t id = spn_qualified_name_to_pkg_id(name);
  return spn_pkg_id_to_qualified_name(id);
}

sp_intern_str_t spn_pkg_canonicalize_pair(sp_str_t namespace, sp_str_t name) {
  return spn_pkg_id_to_qualified_name((spn_pkg_id_t) {
    .name = name,
    .namespace = namespace
  });
}

sp_intern_str_t spn_pkg_id_to_qualified_name(spn_pkg_id_t id) {
  sp_str_t namespace = sp_str_empty(id.namespace) ? spn_intern_lit("core") : id.namespace;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t qualified = spn_intern(sp_str_join(scratch.mem, namespace, id.name, strl("/")));
  sp_mem_end_scratch(scratch);
  return qualified;
}

spn_pkg_id_t spn_qualified_name_to_pkg_id(sp_str_t qualified) {
  sp_str_pair_t pair = sp_str_cleave_c8(qualified, '/');

  if (sp_str_empty(pair.second)) {
    return (spn_pkg_id_t) {
      .namespace = spn_intern_lit("core"),
      .name = spn_intern(qualified)
    };
  }

  return (spn_pkg_id_t) {
    .namespace = spn_intern(pair.first),
    .name = spn_intern(pair.second)
  };
}
