#include "sp.h"
#include "sp/macro.h"
#include "git/key.h"
#include "git/url.h"

sp_str_t spn_git_db_key(sp_mem_t mem, sp_str_t url) {
  sp_str_t name = spn_git_url_name(url);
  sp_hash_t hash = sp_hash_bytes(url.data, url.len, 0);
  return sp_fmt(mem, "{}-{:0>16x}", sp_fmt_str(name), sp_fmt_uint(hash)).value;
}

sp_str_t spn_git_checkout_key(sp_mem_t mem, spn_git_checkout_id_t id) {
  sp_str_t name = spn_git_url_name(id.url);

  sp_hash_t parts[4] = {
    sp_hash_bytes(id.url.data, id.url.len, 0),
    sp_hash_bytes(id.rev.data, id.rev.len, 0),
    sp_hash_bytes(id.dir.data, id.dir.len, 0),
    id.patches.hash,
  };
  u32 num_parts = id.patches.hash ? 4 : 3;
  sp_hash_t hash = sp_hash_combine(parts, num_parts);

  return sp_fmt(mem, "{}-{:0>16x}", sp_fmt_str(name), sp_fmt_uint(hash)).value;
}
