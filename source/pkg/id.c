#include "intern/intern.h"
#include "pkg/id.h"
#include "sp/macro.h"

sp_str_t spn_pkg_canonicalize_name(sp_str_t name) {
  if (sp_str_empty(name)) return sp_zero_struct(sp_str_t);
  spn_pkg_id_t id = spn_qualified_name_to_pkg_id(name);
  return spn_pkg_id_to_qualified_name(id);
}

sp_str_t spn_pkg_canonicalize_pair(sp_str_t namespace, sp_str_t name) {
  return spn_pkg_id_to_qualified_name((spn_pkg_id_t) {
    .name = name,
    .namespace = namespace
  });
}

sp_str_t spn_pkg_id_to_qualified_name(spn_pkg_id_t id) {
  sp_str_t namespace = sp_str_empty(id.namespace) ? spn_intern_lit("core") : id.namespace;
  sp_str_t qualified = sp_str_join(namespace, id.name, strl("/"));
  return spn_intern(qualified);
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
