#ifndef SPN_SP_MACRO_H
#define SPN_SP_MACRO_H

// One shared OS page allocator. Still backs the lazy-init container shims in
// compat.h and the sp_format family below; direct uses are being threaded to a
// real sp_mem_t (spn.mem and friends) and should disappear from leaf code.
#define spn_allocator sp_mem_os_new()

// MARKER: an allocator threading decision was deferred here. Functionally a
// fresh OS allocator (same as spn_allocator), but grep for spn_mem_todo to find
// every site that still needs a real sp_mem_t threaded in.
#define spn_mem_todo sp_mem_os_new()

// Formatting: spn predates the sp_fmt rename. Route the old spelling through the new
// API with the shared allocator. The new directive grammar lives in the format literals.
#define sp_format(...) (sp_fmt(spn_allocator, __VA_ARGS__).value)
#define sp_format_str(fmt, ...) (sp_fmt(spn_allocator, sp_str_to_cstr(spn_allocator, (fmt)), ##__VA_ARGS__).value)

#define SP_FMT_STR(V)        sp_fmt_str(V)
#define SP_FMT_CSTR(V)       sp_fmt_cstr(V)
#define SP_FMT_PTR(V)        sp_fmt_ptr(V)
#define SP_FMT_U8(V)         sp_fmt_uint(V)
#define SP_FMT_U16(V)        sp_fmt_uint(V)
#define SP_FMT_U32(V)        sp_fmt_uint(V)
#define SP_FMT_U64(V)        sp_fmt_uint(V)
#define SP_FMT_S8(V)         sp_fmt_int(V)
#define SP_FMT_S16(V)        sp_fmt_int(V)
#define SP_FMT_S32(V)        sp_fmt_int(V)
#define SP_FMT_S64(V)        sp_fmt_int(V)
#define SP_FMT_F32(V)        sp_fmt_float(V)
#define SP_FMT_F64(V)        sp_fmt_float(V)
static inline sp_str_t spn_format_hash_hex(sp_hash_t hash) {
  static const c8 digits[] = "0123456789abcdef";
  c8* buf = (c8*)sp_alloc(spn_allocator, 16);
  for (s32 i = 15; i >= 0; i--) {
    buf[i] = digits[hash & 0xF];
    hash >>= 4;
  }
  return sp_str(buf, 16);
}
#define SP_FMT_HASH(V)       sp_fmt_str(spn_format_hash_hex(V))
#define SP_FMT_QUOTED_STR(V) sp_fmt_str(V)
#define SP_FMT_QSTR(STR)     sp_fmt_str(STR)
#define sp_fmt_qstr(STR)     sp_fmt_str(STR)
#define SP_FMT_QCSTR(CSTR)   sp_fmt_cstr(CSTR)

// Zero-init: SP_ZERO_INITIALIZE() was renamed to the bare sp_zero literal.
#define SP_ZERO_INITIALIZE() sp_zero
#define SP_ZERO()            sp_zero

#define SP_ALLOC(T) sp_alloc_type(spn_allocator, T)

// Transient process: we leak on purpose. The OS page allocator frees via
// munmap and needs the exact size, which spn does not track, so spn-side frees
// are no-ops rather than risk an incorrect munmap.
#define spn_free(ptr) ((void)(ptr))

#define _SP_MSTR(x) #x
#define SP_MSTR(x) _SP_MSTR(x)
#define _SP_MCAT(x, y) x##y
#define SP_MCAT(x, y) _SP_MCAT(x, y)
#define strl(literal) sp_str_lit(literal)
#define SP_LIT(literal) sp_str_lit(literal)

#define sp_arena_alloc_type(arena, type) sp_mem_arena_alloc((arena), sizeof((type)))

// Renamed/removed sp symbols routed to their new spellings.
#define SP_LOG(...)            sp_log(__VA_ARGS__)
#define SP_FATAL(...)          sp_fatal(__VA_ARGS__)
#define SP_ZERO_STRUCT(T)      sp_zero_s(T)
#define sp_zero_struct(T)      sp_zero_s(T)

#define sp_dyn_array(T)        sp_da(T)
#define sp_dyn_array_push(a, v) sp_da_push(a, v)
#define sp_dyn_array_pop(a)     sp_da_pop(a)
#define sp_dyn_array_size(a)    sp_da_size(a)
#define sp_dyn_array_empty(a)   sp_da_empty(a)
#define sp_dyn_array_for(a, it) sp_da_for(a, it)
#define sp_dyn_array_sort(a, f) sp_da_sort(a, f)

// One shared allocator: the old per-scope context stack is gone, so pushing/
// popping an arena is a no-op (every allocation already targets spn_allocator).
#define sp_context_push_arena(a)     ((void)(a))
#define sp_context_push_allocator(a) ((void)(a))
#define sp_context_pop()             ((void)0)

#define sp_str_ht_exists(ht, key) (sp_str_ht_get((ht), (key)) != SP_NULLPTR)
#define sp_ht_key_exists(ht, key) (sp_ht_getp((ht), (key)) != SP_NULLPTR)

#define sp_zero_initialize() sp_zero
#define SP_CSTR(s)           sp_str_view(s)
#define SP_MAX(a, b)         sp_max((a), (b))
#define SP_MIN(a, b)         sp_min((a), (b))
#define SP_ASSERT_FMT(cond, ...) sp_assert(cond)
#define sp_broken(...)       sp_assert(false)
#define sp_format_v(fmt, args) (sp_fmt_mem_v(spn_allocator, (fmt), (args)).value)
#define sp_assert_fmt(cond, ...) sp_assert(cond)

// The *_LOWER enum-case codegen helpers were dropped from sp.h; restore them.
#define SP_X_ENUM_CASE_TO_STRING_LOWER(ID) \
  case ID: { return sp_str_lit(SP_MACRO_STR(ID)); }
#define SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER(ID, NAME) \
  case ID: { return sp_str_lit(NAME); }

#endif
