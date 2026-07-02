#include "sp.h"
#include "sp/macro.h"
#include "git/key.h"

sp_str_t spn_git_url_name(sp_str_t url) {
  if (sp_str_ends_with(url, SP_LIT("/"))) {
    url = sp_str_prefix(url, url.len - 1);
  }

  if (sp_str_ends_with(url, SP_LIT(".git"))) {
    url = sp_str_prefix(url, url.len - 4);
  }

  u64 last_sep = 0;
  sp_for(it, url.len) {
    if (url.data[it] == '/' || url.data[it] == ':') {
      last_sep = it + 1;
    }
  }

  return sp_str_suffix(url, url.len - last_sep);
}

sp_str_t spn_git_db_key(sp_mem_t mem, sp_str_t url) {
  sp_str_t name = spn_git_url_name(url);
  sp_hash_t hash = sp_hash_bytes(url.data, url.len, 0);
  return sp_fmt(mem, "{}-{:0>16x}", sp_fmt_str(name), sp_fmt_uint(hash)).value;
}

sp_str_t spn_git_checkout_key(sp_mem_t mem, sp_str_t url, sp_str_t rev, sp_str_t dir) {
  sp_str_t name = spn_git_url_name(url);

  sp_hash_t parts[] = {
    sp_hash_bytes(url.data, url.len, 0),
    sp_hash_bytes(rev.data, rev.len, 0),
    sp_hash_bytes(dir.data, dir.len, 0),
  };
  sp_hash_t hash = sp_hash_combine(parts, SP_CARR_LEN(parts));

  return sp_fmt(mem, "{}-{:0>16x}", sp_fmt_str(name), sp_fmt_uint(hash)).value;
}
