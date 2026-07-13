#ifndef SPN_SP_MACRO_H
#define SPN_SP_MACRO_H

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
#define SP_FMT_QUOTED_STR(V) sp_fmt_str(V)
#define SP_FMT_QSTR(STR)     sp_fmt_str(STR)
#define sp_fmt_qstr(STR)     sp_fmt_str(STR)
#define SP_FMT_QCSTR(CSTR)   sp_fmt_cstr(CSTR)

#define SP_ZERO()            sp_zero
#define sp_zero_initialize() sp_zero
#define SP_ZERO_STRUCT(T)    sp_zero_s(T)
#define sp_zero_struct(T)    sp_zero_s(T)

#define _SP_MSTR(x) #x
#define SP_MSTR(x) _SP_MSTR(x)
#define _SP_MCAT(x, y) x##y
#define SP_MCAT(x, y) _SP_MCAT(x, y)
#define strl(literal) sp_str_lit(literal)
#define SP_LIT(literal) sp_str_lit(literal)

#define SP_LOG(...)   sp_log(__VA_ARGS__)
#define SP_FATAL(...) sp_fatal(__VA_ARGS__)

#define sp_str_ht_exists(ht, key) (sp_str_ht_get((ht), (key)) != SP_NULLPTR)
#define sp_ht_key_exists(ht, key) (sp_ht_getp((ht), (key)) != SP_NULLPTR)

#define SP_CSTR(s)   sp_str_view(s)
#define SP_MAX(a, b) sp_max((a), (b))
#define SP_MIN(a, b) sp_min((a), (b))
#define SP_ASSERT_FMT(cond, ...) sp_assert(cond)
#define sp_broken(...) sp_assert(false)

#define SP_X_ENUM_CASE_TO_STRING_LOWER(ID) \
  case ID: { return sp_str_lit(SP_MACRO_STR(ID)); }
#define SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER(ID, NAME) \
  case ID: { return sp_str_lit(NAME); }

#endif
