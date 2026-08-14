#ifndef SPN_SP_STR_H
#define SPN_SP_STR_H

#include "sp.h"

typedef struct {
  sp_str_t str;
  sp_str_t line;
  u32 index;
  u32 cursor;
  bool done;
} sp_str_line_it_t;

sp_str_t  sp_str_repeat(sp_mem_t mem, c8 c, u32 len);
sp_hash_t sp_hash_str(sp_str_t str);

bool             sp_str_line_it_valid(const sp_str_line_it_t* it);
void             sp_str_line_it_next(sp_str_line_it_t* it);
sp_str_line_it_t sp_str_line_it_begin(sp_str_t str);

#define sp_str_for_line(str, it) \
  for (sp_str_line_it_t it = sp_str_line_it_begin((str)); sp_str_line_it_valid(&(it)); sp_str_line_it_next(&(it)))

#endif
