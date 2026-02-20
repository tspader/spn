#ifndef SPN_SP_MACRO_H
#define SPN_SP_MACRO_H

#define SP_FMT_QSTR(STR) SP_FMT_QUOTED_STR(STR)
#define SP_FMT_QCSTR(CSTR) SP_FMT_QUOTED_STR(sp_str_view(CSTR))
#define SP_ALLOC(T) (T*)sp_alloc(sizeof(T))
#define _SP_MSTR(x) #x
#define SP_MSTR(x) _SP_MSTR(x)
#define _SP_MCAT(x, y) x##y
#define SP_MCAT(x, y) _SP_MCAT(x, y)
#define SP_ZERO() SP_ZERO_INITIALIZE()

#define sp_try_goto(expr, label) do { s32 _sp_result = (expr); if (_sp_result) { goto label; }; } while (0)

#endif
