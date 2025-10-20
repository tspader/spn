#define SP_IMPLEMENTATION
#include "sp.h"

#include "stdio.h"

#include "git2.h"

s32 main(s32 num_args, const c8** args) {
  sp_init_default();
  sp_str_t exe = sp_os_get_executable_path();
  SP_LOG("{:fg brightred}", SP_FMT_STR(exe));

  for (c8** envp = (c8**)environ; *envp != SP_NULLPTR; envp++) {
    SP_LOG("{:fg brightcyan}", SP_FMT_CSTR(*envp));
    sp_dyn_array(sp_str_t) parts = sp_str_split_c8(sp_str_view(*envp), '=');
    sp_dyn_array_for(parts, i) {
      SP_LOG("  {:fg brightyellow}", SP_FMT_STR(parts[i]));
    }
  }

  git_repository* repo = SP_NULLPTR;
  git_repository_open(&repo, ".");
}
