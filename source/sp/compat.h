#ifndef SPN_SP_COMPAT_H
#define SPN_SP_COMPAT_H

// Force-included into every spn TU. Pulls sp.h first so the compat types below
// resolve, then the macro shims. Lets spn keep its pre-rename spellings while
// targeting the new single-header sp.h.
#include "sp.h"
#include "sp/macro.h"

// ---------------------------------------------------------------------------
// String builder (removed from sp.h in favor of sp_fmt / dyn-mem writers).
// ---------------------------------------------------------------------------
typedef struct {
  sp_io_dyn_mem_writer_t writer;
  bool initialized;
  struct { sp_str_t word; u32 level; } indent;
} sp_str_builder_t;

void            sp_str_builder_append(sp_str_builder_t* b, sp_str_t str);
void            sp_str_builder_append_cstr(sp_str_builder_t* b, const c8* str);
void            sp_str_builder_append_c8(sp_str_builder_t* b, c8 c);
void            sp_str_builder_append_fmt(sp_str_builder_t* b, const c8* fmt, ...);
void            sp_str_builder_append_fmt_str(sp_str_builder_t* b, sp_str_t fmt, ...);
void            sp_str_builder_new_line(sp_str_builder_t* b);
void            sp_str_builder_indent(sp_str_builder_t* b);
void            sp_str_builder_dedent(sp_str_builder_t* b);
sp_str_t        sp_str_builder_as_str(sp_str_builder_t* b);
sp_str_t        sp_str_builder_to_str(sp_str_builder_t* b);
sp_mem_buffer_t sp_str_builder_into_buffer(sp_str_builder_t* b);
void            sp_str_builder_free(sp_str_builder_t* b);

// ---------------------------------------------------------------------------
// IO writer/reader (split into stateful structs in sp.h; compat constructors
// heap-allocate the concrete writer/reader and return the embedded base ptr).
// ---------------------------------------------------------------------------
typedef enum {
  SP_IO_WRITE_MODE_OVERWRITE,
  SP_IO_WRITE_MODE_APPEND,
} sp_io_write_mode_t;

sp_io_writer_t* sp_io_writer_from_file(sp_str_t path, sp_io_write_mode_t mode);
sp_io_writer_t* sp_io_writer_from_fd(s32 fd, sp_io_close_mode_t close_mode);
sp_io_writer_t* sp_io_writer_from_dyn_mem(void);
void            sp_io_writer_close(sp_io_writer_t* w);
u64             sp_io_writer_size(sp_io_writer_t* w);
sp_str_t        sp_io_writer_to_str(sp_io_writer_t* w);

sp_io_reader_t* sp_io_reader_from_file(sp_str_t path);
u64             sp_io_reader_size(sp_io_reader_t* r);
void            sp_io_reader_close(sp_io_reader_t* r);

#define sp_io_writer_from_dyn_mem_ex(buffer, size, allocator) sp_io_writer_from_dyn_mem()

// Test helpers for inline read-file checks (sp_io_read_file is now 3-arg).
static inline sp_str_t spn_compat_read_file(sp_str_t path) {
  sp_str_t s = sp_zero;
  sp_io_read_file(spn_allocator, path, &s);
  return s;
}
static inline bool spn_test_read_eq(sp_str_t path, const c8* expect) {
  return sp_str_equal(spn_compat_read_file(path), sp_str_view(expect));
}
static inline bool spn_test_read_empty(sp_str_t path) {
  return sp_str_empty(spn_compat_read_file(path));
}

// spn zero-inits its containers and shares one page allocator; sp_da_push
// asserts on a NULL array, so lazily initialize on first push. Harmless for
// already-initialized arrays (including sp.h's own, which are never NULL here).
#ifdef sp_da_push
  #undef sp_da_push
#endif
#define sp_da_push(__ARR, __VAL) do { \
  if (!(__ARR)) sp_da_init(spn_allocator, (__ARR)); \
  *sp_da_vp(__ARR) = sp_da_grow(__ARR, 1); \
  (__ARR)[sp_da_head(__ARR)->size++] = (__VAL); \
} while(0)

// Same for hash tables: set_fns/insert dereference the table, so lazily init a
// NULL one. getp/get_ex already guard NULL, so reads are safe pre-init.
#ifdef sp_ht_set_fns
  #undef sp_ht_set_fns
#endif
#define sp_ht_set_fns(ht, hash_fn, cmp_fn) do { \
  if (!(ht)) sp_ht_init(spn_allocator, (ht)); \
  (ht)->info.fn.hash = (hash_fn); \
  (ht)->info.fn.compare = (cmp_fn); \
} while(0)

#ifdef sp_ht_insert
  #undef sp_ht_insert
#endif
#define sp_ht_insert(ht, k, v) do { \
  if (!(ht)) sp_ht_init(spn_allocator, (ht)); \
  sp_ht_insert_ex(ht, k, v); \
} while(0)

#ifdef sp_str_ht_insert
  #undef sp_str_ht_insert
#endif
#define sp_str_ht_insert(ht, k, v) do { \
  if (!(ht)) sp_str_ht_init(spn_allocator, (ht)); \
  sp_ht_insert_ex(ht, k, v); \
} while(0)

// Ring buffers: lazily init on first push (asserts non-NULL otherwise).
#ifdef sp_rb_push
  #undef sp_rb_push
#endif
#define sp_rb_push(__ARR, __VAL) do { \
  if (!(__ARR)) sp_rb_init(spn_allocator, (__ARR)); \
  if (sp_rb_full(__ARR)) { \
    if (sp_rb_mode(__ARR) == SP_RQ_MODE_OVERWRITE) { \
      sp_rb_head(__ARR)->head = (sp_rb_head(__ARR)->head + 1) % sp_rb_capacity(__ARR); \
      sp_rb_head(__ARR)->size--; \
    } else { \
      *((void**)&(__ARR)) = sp_rb_grow_ex(__ARR, sizeof(*(__ARR)), sp_rb_capacity(__ARR) * 2u); \
    } \
  } \
  u32 __sp_rb_tail = (sp_rb_head(__ARR)->head + sp_rb_head(__ARR)->size) % sp_rb_capacity(__ARR); \
  (__ARR)[__sp_rb_tail] = (__VAL); \
  sp_rb_head(__ARR)->size++; \
} while (0)

#endif
