#ifndef SP_TEMPLATE_H
#define SP_TEMPLATE_H

#include "sp.h"

#ifndef SP_TEMPLATE_MAX_DEPTH
  #define SP_TEMPLATE_MAX_DEPTH 32
#endif

typedef struct sp_template_scope sp_template_scope_t;
typedef struct sp_template_registry sp_template_registry_t;

typedef enum {
  SP_TEMPLATE_OK = 0,
  SP_TEMPLATE_ERR_IO,
  SP_TEMPLATE_ERR_UNTERMINATED,
  SP_TEMPLATE_ERR_UNTERMINATED_FOR,
  SP_TEMPLATE_ERR_UNEXPECTED_END,
  SP_TEMPLATE_ERR_BAD_DIRECTIVE,
  SP_TEMPLATE_ERR_MISSING_KEY,
  SP_TEMPLATE_ERR_WRONG_KIND,
  SP_TEMPLATE_ERR_MISSING_TEMPLATE,
  SP_TEMPLATE_ERR_RECURSION,
  SP_TEMPLATE_ERR_DUPLICATE,
} sp_template_err_t;

typedef enum {
  SP_TEMPLATE_VALUE_STR,
  SP_TEMPLATE_VALUE_LIST,
} sp_template_value_kind_t;

typedef struct {
  sp_str_t name;
  sp_template_value_kind_t kind;
  union {
    sp_str_t str;
    sp_da(sp_template_scope_t*) list;
  } as;
} sp_template_value_t;

struct sp_template_scope {
  sp_mem_t mem;
  sp_template_scope_t* parent;
  sp_da(sp_template_value_t) values;
};

typedef struct {
  sp_str_t name;
  sp_str_t src;
} sp_template_entry_t;

struct sp_template_registry {
  sp_mem_t mem;
  sp_da(sp_template_entry_t) entries;
};

SP_API sp_template_scope_t* sp_template_scope_create(sp_mem_t mem);
SP_API void                 sp_template_set(sp_template_scope_t* scope, sp_str_t name, sp_str_t value);
SP_API void                 sp_template_list(sp_template_scope_t* scope, sp_str_t name);
SP_API sp_template_scope_t* sp_template_push(sp_template_scope_t* scope, sp_str_t name);

SP_API sp_template_registry_t* sp_template_registry_create(sp_mem_t mem);
SP_API sp_template_err_t       sp_template_register(sp_template_registry_t* reg, sp_str_t name, sp_str_t src);
SP_API bool                    sp_template_get(sp_template_registry_t* reg, sp_str_t name, sp_str_t* out);
SP_API sp_template_err_t       sp_template_load_dir(sp_template_registry_t* reg, sp_str_t root);

SP_API sp_template_err_t    sp_template_render(sp_io_writer_t* io, sp_str_t tmpl, sp_template_scope_t* scope, sp_template_registry_t* reg);

#endif // SP_TEMPLATE_H


#if defined(SP_IMPLEMENTATION) && !defined(SP_TEMPLATE_IMPLEMENTATION)
  #define SP_TEMPLATE_IMPLEMENTATION
#endif

#if defined(SP_TEMPLATE_IMPLEMENTATION)

static sp_template_value_t* sp_template_scope_find(sp_template_scope_t* scope, sp_str_t name) {
  sp_da_for(scope->values, i) {
    if (sp_str_equal(scope->values[i].name, name)) return &scope->values[i];
  }
  return SP_NULLPTR;
}

static sp_template_value_t* sp_template_scope_resolve(sp_template_scope_t* scope, sp_str_t name) {
  for (sp_template_scope_t* s = scope; s; s = s->parent) {
    sp_template_value_t* value = sp_template_scope_find(s, name);
    if (value) return value;
  }
  return SP_NULLPTR;
}

sp_template_scope_t* sp_template_scope_create(sp_mem_t mem) {
  sp_template_scope_t* scope = sp_alloc_type(mem, sp_template_scope_t);
  scope->mem = mem;
  scope->parent = SP_NULLPTR;
  scope->values = sp_da_new(mem, sp_template_value_t);
  return scope;
}

void sp_template_set(sp_template_scope_t* scope, sp_str_t name, sp_str_t value) {
  sp_template_value_t* existing = sp_template_scope_find(scope, name);
  if (existing) {
    existing->kind = SP_TEMPLATE_VALUE_STR;
    existing->as.str = value;
    return;
  }

  sp_template_value_t entry = sp_zero;
  entry.name = name;
  entry.kind = SP_TEMPLATE_VALUE_STR;
  entry.as.str = value;
  sp_da_push(scope->values, entry);
}

static sp_template_value_t* sp_template_ensure_list(sp_template_scope_t* scope, sp_str_t name) {
  sp_template_value_t* existing = sp_template_scope_find(scope, name);
  if (existing) return existing;

  sp_template_value_t entry = sp_zero;
  entry.name = name;
  entry.kind = SP_TEMPLATE_VALUE_LIST;
  entry.as.list = sp_da_new(scope->mem, sp_template_scope_t*);
  sp_da_push(scope->values, entry);
  return &scope->values[sp_da_size(scope->values) - 1];
}

void sp_template_list(sp_template_scope_t* scope, sp_str_t name) {
  sp_template_ensure_list(scope, name);
}

sp_template_scope_t* sp_template_push(sp_template_scope_t* scope, sp_str_t name) {
  sp_template_value_t* list = sp_template_ensure_list(scope, name);
  sp_template_scope_t* child = sp_template_scope_create(scope->mem);
  child->parent = scope;
  sp_da_push(list->as.list, child);
  return child;
}

sp_template_registry_t* sp_template_registry_create(sp_mem_t mem) {
  sp_template_registry_t* reg = sp_alloc_type(mem, sp_template_registry_t);
  reg->mem = mem;
  reg->entries = sp_da_new(mem, sp_template_entry_t);
  return reg;
}

static sp_template_entry_t* sp_template_registry_find(sp_template_registry_t* reg, sp_str_t name) {
  sp_da_for(reg->entries, i) {
    if (sp_str_equal(reg->entries[i].name, name)) return &reg->entries[i];
  }
  return SP_NULLPTR;
}

sp_template_err_t sp_template_register(sp_template_registry_t* reg, sp_str_t name, sp_str_t src) {
  if (sp_template_registry_find(reg, name)) return SP_TEMPLATE_ERR_DUPLICATE;
  sp_template_entry_t entry = sp_zero;
  entry.name = name;
  entry.src = src;
  sp_da_push(reg->entries, entry);
  return SP_TEMPLATE_OK;
}

bool sp_template_get(sp_template_registry_t* reg, sp_str_t name, sp_str_t* out) {
  if (!reg) return false;
  sp_template_entry_t* entry = sp_template_registry_find(reg, name);
  if (!entry) return false;
  if (out) *out = entry->src;
  return true;
}

sp_template_err_t sp_template_load_dir(sp_template_registry_t* reg, sp_str_t root) {
  sp_template_err_t err = SP_TEMPLATE_OK;

  sp_str_t norm_root = sp_fs_normalize_path(reg->mem, root);
  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(reg->mem, root);

  sp_da_for(entries, i) {
    sp_fs_entry_t* e = &entries[i];
    if (e->kind != SP_FS_KIND_FILE) continue;
    if (e->name.len && e->name.data[0] == '.') continue; // dotfile

    sp_str_t path = sp_fs_normalize_path(reg->mem, e->path);
    if (path.len <= norm_root.len) continue;
    sp_str_t rel = sp_str_sub(path, (s32)norm_root.len, (s32)(path.len - norm_root.len));
    while (rel.len && rel.data[0] == '/') rel = sp_str_sub(rel, 1, (s32)rel.len - 1);

    sp_str_t ext = sp_fs_get_ext(rel);
    sp_str_t key = ext.len ? sp_str_sub(rel, 0, (s32)(rel.len - ext.len - 1)) : rel;

    sp_str_t src = sp_zero;
    sp_try_as(sp_io_read_file(reg->mem, e->path, &src), SP_TEMPLATE_ERR_IO);
    err = sp_template_register(reg, key, src);
    if (err) return err;
  }

  return SP_TEMPLATE_OK;
}

typedef struct {
  sp_str_t str;
  u32 i;
} sp_template_parser_t;

typedef enum {
  SP_TEMPLATE_DIR_SUBST,
  SP_TEMPLATE_DIR_INCLUDE,
  SP_TEMPLATE_DIR_FOR,
  SP_TEMPLATE_DIR_END,
} sp_template_dir_kind_t;

typedef struct {
  sp_template_dir_kind_t kind;
  sp_str_t name; // bound for SUBST and FOR
} sp_template_dir_t;

static c8 sp_template_peek(sp_template_parser_t* p, u32 offset) {
  u32 index = p->i + offset;
  if (index >= p->str.len) return 0;
  return p->str.data[index];
}

static bool sp_template_at(sp_template_parser_t* p, c8 a, c8 b) {
  return sp_template_peek(p, 0) == a && sp_template_peek(p, 1) == b;
}

// A directive name is a single non-empty run with no interior whitespace.
static bool sp_template_name_valid(sp_str_t name) {
  if (!name.len) return false;
  sp_str_for(name, i) {
    c8 c = name.data[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return false;
  }
  return true;
}

static sp_template_err_t sp_template_classify(sp_str_t content, sp_template_dir_t* out) {
  sp_str_t directive = sp_str_trim(content);

  if (directive.len && directive.data[0] == '.') {
    sp_str_t name = sp_str_sub(directive, 1, (s32)directive.len - 1);
    if (!sp_template_name_valid(name)) return SP_TEMPLATE_ERR_BAD_DIRECTIVE;
    out->kind = SP_TEMPLATE_DIR_SUBST;
    out->name = name;
    return SP_TEMPLATE_OK;
  }

  if (directive.len && directive.data[0] == '$') {
    sp_str_t name = sp_str_sub(directive, 1, (s32)directive.len - 1);
    if (!sp_template_name_valid(name)) return SP_TEMPLATE_ERR_BAD_DIRECTIVE;
    out->kind = SP_TEMPLATE_DIR_INCLUDE;
    out->name = name;
    return SP_TEMPLATE_OK;
  }

  sp_str_pair_t pair = sp_str_cleave_c8(directive, ' ');
  sp_str_t keyword = pair.first;
  sp_str_t rest = sp_str_trim(pair.second);

  if (sp_str_equal_cstr(keyword, "for")) {
    if (!rest.len || rest.data[0] != '.') return SP_TEMPLATE_ERR_BAD_DIRECTIVE;
    sp_str_t name = sp_str_sub(rest, 1, (s32)rest.len - 1);
    if (!sp_template_name_valid(name)) return SP_TEMPLATE_ERR_BAD_DIRECTIVE;
    out->kind = SP_TEMPLATE_DIR_FOR;
    out->name = name;
    return SP_TEMPLATE_OK;
  }

  if (sp_str_equal_cstr(keyword, "end")) {
    if (rest.len) return SP_TEMPLATE_ERR_BAD_DIRECTIVE;
    out->kind = SP_TEMPLATE_DIR_END;
    return SP_TEMPLATE_OK;
  }

  return SP_TEMPLATE_ERR_BAD_DIRECTIVE;
}

static sp_str_t sp_template_lstrip_block(sp_str_t run) {
  u32 tail = run.len;
  while (tail > 0 && (run.data[tail - 1] == ' ' || run.data[tail - 1] == '\t')) tail--;
  if (tail == 0) return run;                  // no preceding newline -> not line-leading
  if (run.data[tail - 1] != '\n') return run; // significant trailing text -> leave it
  return sp_str_sub(run, 0, (s32)tail);        // drop the indentation, keep the newline
}

static void sp_template_trim_after_block(sp_template_parser_t* p) {
  u32 i = p->i;
  while (i < p->str.len && (p->str.data[i] == ' ' || p->str.data[i] == '\t')) i++;
  if (i < p->str.len && p->str.data[i] == '\r') i++;
  if (i < p->str.len && p->str.data[i] == '\n') p->i = i + 1;
}

static sp_template_err_t sp_template_render_block(
  sp_template_parser_t* p,
  sp_template_scope_t* scope,
  sp_io_writer_t* io,
  sp_template_registry_t* reg,
  u32 depth,
  bool* hit_end
) {
  sp_template_err_t err = SP_TEMPLATE_OK;

  *hit_end = false;

  while (p->i < p->str.len) {
    // Scan the literal run up to the next directive (emitted once we know what
    // follows it -- a block directive lstrips the run's trailing indentation).
    u32 literal = p->i;
    while (p->i < p->str.len && !sp_template_at(p, '{', '{')) p->i++;
    u32 literal_end = p->i;
    if (p->i >= p->str.len) {
      if (io && literal_end > literal) {
        sp_str_t run = sp_str_sub(p->str, (s32)literal, (s32)(literal_end - literal));
        sp_try_as(sp_io_write_str(io, run, SP_NULLPTR), SP_TEMPLATE_ERR_IO);
      }
      return SP_TEMPLATE_OK;
    }

    // Consume '{{ ... }}'.
    p->i += 2;
    u32 content = p->i;
    while (p->i < p->str.len && !sp_template_at(p, '}', '}')) p->i++;
    if (p->i >= p->str.len) return SP_TEMPLATE_ERR_UNTERMINATED;
    sp_str_t directive = sp_str_sub(p->str, (s32)content, (s32)(p->i - content));
    p->i += 2;

    sp_template_dir_t dir = sp_zero;
    err = sp_template_classify(directive, &dir);
    if (err) return err;

    bool block = dir.kind == SP_TEMPLATE_DIR_FOR || dir.kind == SP_TEMPLATE_DIR_END;

    if (io && literal_end > literal) {
      sp_str_t run = sp_str_sub(p->str, (s32)literal, (s32)(literal_end - literal));
      if (block) run = sp_template_lstrip_block(run);
      if (run.len) sp_try_as(sp_io_write_str(io, run, SP_NULLPTR), SP_TEMPLATE_ERR_IO);
    }
    if (block) sp_template_trim_after_block(p);

    switch (dir.kind) {
      case SP_TEMPLATE_DIR_SUBST: {
        if (io) {
          sp_template_value_t* value = sp_template_scope_resolve(scope, dir.name);
          if (!value) return SP_TEMPLATE_ERR_MISSING_KEY;
          if (value->kind != SP_TEMPLATE_VALUE_STR) return SP_TEMPLATE_ERR_WRONG_KIND;
          sp_try_as(sp_io_write_str(io, value->as.str, SP_NULLPTR), SP_TEMPLATE_ERR_IO);
        }
        break;
      }

      case SP_TEMPLATE_DIR_INCLUDE: {
        if (io) {
          if (depth >= SP_TEMPLATE_MAX_DEPTH) return SP_TEMPLATE_ERR_RECURSION;

          sp_template_value_t* value = sp_template_scope_resolve(scope, dir.name);
          if (!value) return SP_TEMPLATE_ERR_MISSING_KEY;
          if (value->kind != SP_TEMPLATE_VALUE_STR) return SP_TEMPLATE_ERR_WRONG_KIND;

          sp_str_t src = sp_zero;
          if (!sp_template_get(reg, value->as.str, &src)) return SP_TEMPLATE_ERR_MISSING_TEMPLATE;

          // Chomp one trailing newline (and a preceding '\r') so a leaf authored as an
          // ordinary newline-terminated file renders as if it had none -- the parent
          // template supplies the separating newline, exactly as a substitution would.
          if (src.len && src.data[src.len - 1] == '\n') {
            src.len--;
            if (src.len && src.data[src.len - 1] == '\r') src.len--;
          }

          sp_template_parser_t leaf = { .str = src, .i = 0 };
          bool leaf_end = false;
          err = sp_template_render_block(&leaf, scope, io, reg, depth + 1, &leaf_end);
          if (err) return err;
          if (leaf_end) return SP_TEMPLATE_ERR_UNEXPECTED_END;
        }
        break;
      }

      case SP_TEMPLATE_DIR_FOR: {
        u32 body = p->i;

        if (!io) {
          // Skip mode: walk the body once to land past the matching 'end'.
          bool body_end = false;
          err = sp_template_render_block(p, SP_NULLPTR, SP_NULLPTR, reg, depth, &body_end);
          if (err) return err;
          if (!body_end) return SP_TEMPLATE_ERR_UNTERMINATED_FOR;
          break;
        }

        sp_template_value_t* value = sp_template_scope_resolve(scope, dir.name);
        if (!value) return SP_TEMPLATE_ERR_MISSING_KEY;
        if (value->kind != SP_TEMPLATE_VALUE_LIST) return SP_TEMPLATE_ERR_WRONG_KIND;

        if (sp_da_empty(value->as.list)) {
          // No children: skip the body, but still validate it closes.
          bool body_end = false;
          err = sp_template_render_block(p, SP_NULLPTR, SP_NULLPTR, reg, depth, &body_end);
          if (err) return err;
          if (!body_end) return SP_TEMPLATE_ERR_UNTERMINATED_FOR;
        }
        else {
          sp_da_for(value->as.list, child) {
            p->i = body;
            bool body_end = false;
            err = sp_template_render_block(p, value->as.list[child], io, reg, depth, &body_end);
            if (err) return err;
            if (!body_end) return SP_TEMPLATE_ERR_UNTERMINATED_FOR;
          }
        }
        break;
      }

      case SP_TEMPLATE_DIR_END: {
        *hit_end = true;
        return SP_TEMPLATE_OK;
      }
    }
  }

  return SP_TEMPLATE_OK;
}

sp_template_err_t sp_template_render(sp_io_writer_t* io, sp_str_t tmpl, sp_template_scope_t* scope, sp_template_registry_t* reg) {
  sp_template_parser_t parser = { .str = tmpl, .i = 0 };
  bool hit_end = false;
  sp_template_err_t err = sp_template_render_block(&parser, scope, io, reg, 0, &hit_end);
  if (err) return err;
  if (hit_end) return SP_TEMPLATE_ERR_UNEXPECTED_END;
  return SP_TEMPLATE_OK;
}

#endif // SP_TEMPLATE_IMPLEMENTATION
