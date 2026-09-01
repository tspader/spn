#ifndef SPN_PATHS_PATHS_H
#define SPN_PATHS_PATHS_H

#include "sp.h"
#include "spn/core.h"
#include "spn/types.h"
#include "paths/types.h"

sp_str_t        spn_path_roots_init(spn_path_roots_t* roots, sp_mem_t mem, sp_str_t storage);
sp_str_t        spn_path_roots_set(spn_path_roots_t* roots, sp_mem_t mem, spn_path_root_t kind, sp_str_t dir);
bool            spn_path_roots_intersect(const spn_path_roots_t* roots, sp_str_t dir);
spn_path_root_t spn_path_root_longest(const spn_path_roots_t* roots, sp_str_t str);
bool            spn_path_normal(sp_str_t path);
spn_path_t      spn_path_anchor(sp_mem_t mem, const spn_path_roots_t* roots, spn_path_t path);
spn_path_t      spn_path_canonicalize(sp_mem_t mem, const spn_path_roots_t* roots, spn_path_t path);
spn_path_t      spn_path_make(const spn_path_roots_t* roots, sp_str_t path);
spn_path_t      spn_path_from_root(spn_path_root_t root);
spn_path_t      spn_path_copy(sp_mem_t mem, spn_path_t path);
spn_path_t      spn_path_join(sp_mem_t mem, spn_path_t base, sp_str_t sub);
spn_path_t      spn_path_suffix(sp_mem_t mem, spn_path_t path, sp_str_t suffix);
sp_str_t        spn_path_str(const spn_path_roots_t* roots, sp_mem_t mem, spn_path_t path);
bool            spn_path_empty(spn_path_t path);
bool            spn_path_equal(spn_path_t a, spn_path_t b);
spn_path_rel_t  spn_path_within(spn_path_t base, spn_path_t path);
sp_hash_t       spn_path_hash(spn_path_t path);
sp_hash_t       spn_path_on_hash(void* key, u64 size);
bool            spn_path_on_compare(void* a, void* b, u64 size);
spn_path_root_set_t spn_path_root_mask(spn_path_root_t root);
spn_path_root_set_t spn_path_pinned_roots();

spn_arg_t       spn_arg_lit(sp_str_t value);
spn_arg_t       spn_arg_path(spn_path_t path);
spn_arg_t       spn_arg_glue(sp_str_t prefix, spn_path_t path);
bool            spn_arg_empty(spn_arg_t arg);
sp_str_t        spn_arg_str(const spn_path_roots_t* roots, sp_mem_t mem, spn_arg_t arg);

sp_str_t        spn_path_root_label(spn_path_root_t root);
spn_path_t      spn_tree_root(spn_tree_roots_t roots, spn_tree_t tree);
spn_path_t      spn_tree_path(sp_mem_t mem, const spn_path_roots_t* roots, spn_tree_roots_t tree, spn_tree_t decl, sp_str_t str);
spn_tree_rel_t  spn_tree_rel(spn_tree_roots_t roots, spn_path_t path);

#endif
