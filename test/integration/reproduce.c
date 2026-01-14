#define SP_IMPLEMENTATION
#include "sp.h"
#include "action.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

s32 main(s32 num_args, const c8** args) {
  if (num_args != 2) {
    sp_log(sp_str_lit("usage: {:fg brightyellow} $path"), SP_FMT_CSTR("reproduce"));
    return 1;
  }

  sp_str_t path = sp_str_view(args[1]);
  if (!sp_fs_exists(path)) return 1;

  sp_str_t file = sp_io_read_file(path);
  toml_table_t* toml = toml_parse(sp_str_to_cstr(file), SP_NULLPTR, 0);
  sp_log(file);
  return 0;
}
