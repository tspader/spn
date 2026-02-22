#ifndef SPN_INTERN_H
#define SPN_INTERN_H

#include "sp.h"

sp_str_t spn_intern(sp_str_t str);
sp_str_t spn_intern_cstr(const c8* cstr);
bool spn_intern_is_equal(sp_str_t a, sp_str_t b);
bool spn_intern_is_equal_cstr(sp_str_t str, const c8* cstr);

#endif
