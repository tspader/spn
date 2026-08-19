#include "sp.h"
#include "sp/macro.h"
#include "git/url.h"

typedef struct {
  sp_str_t scheme;
  sp_str_t host;
  sp_str_t path;
} git_url_t;

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

static sp_str_t strip_user(sp_str_t authority) {
  sp_str_pair_t user = sp_str_cleave_c8(authority, '@');
  return sp_str_equal(user.first, authority) ? authority : user.second;
}

static git_url_t parse_authority(sp_mem_t mem, sp_str_t url, s32 scheme_end) {
  sp_str_t scheme = sp_str_to_lower(mem, sp_str_prefix(url, scheme_end));
  sp_str_pair_t split = sp_str_cleave_c8(sp_str_suffix(url, (s32)url.len - scheme_end - 3), '/');
  sp_str_t host = strip_user(split.first);

  if (sp_str_equal(scheme, sp_str_lit("ssh")) && !sp_str_contains(host, sp_str_lit(":"))) {
    return (git_url_t) {
      .scheme = sp_str_lit("https"),
      .host = host,
      .path = split.second,
    };
  }

  if (sp_str_equal(scheme, sp_str_lit("http")) || sp_str_equal(scheme, sp_str_lit("https"))) {
    return (git_url_t) {
      .scheme = scheme,
      .host = host,
      .path = split.second,
    };
  }

  return sp_zero_s(git_url_t);
}

static git_url_t parse_scp(sp_str_t url) {
  s32 colon = sp_str_find_c8(url, ':');
  s32 slash = sp_str_find_c8(url, '/');

  bool no_colon = colon == SP_STR_NO_MATCH;
  bool empty_host = colon == 0;
  bool windows_drive = colon == 1;
  bool slash_before_colon = slash != SP_STR_NO_MATCH && slash < colon;
  if (no_colon || empty_host || windows_drive || slash_before_colon) {
    return sp_zero_s(git_url_t);
  }

  sp_str_pair_t split = sp_str_cleave_c8(url, ':');
  return (git_url_t) {
    .scheme = sp_str_lit("https"),
    .host = strip_user(split.first),
    .path = sp_str_strip_left(split.second, sp_str_lit("/")),
  };
}

static git_url_t parse_url(sp_mem_t mem, sp_str_t url) {
  s32 scheme_end = sp_str_find(url, sp_str_lit("://"));
  git_url_t parsed = scheme_end == SP_STR_NO_MATCH ? parse_scp(url) : parse_authority(mem, url, scheme_end);
  if (sp_str_empty(parsed.host) || sp_str_empty(parsed.path)) {
    return sp_zero_s(git_url_t);
  }
  return parsed;
}

sp_str_t spn_git_url_web(sp_mem_t mem, sp_str_t url) {
  git_url_t parsed = parse_url(mem, url);
  if (sp_str_empty(parsed.scheme)) {
    return url;
  }

  return sp_fmt(mem, "{}://{}/{}",
    sp_fmt_str(parsed.scheme),
    sp_fmt_str(sp_str_to_lower(mem, parsed.host)),
    sp_fmt_str(parsed.path)
  ).value;
}
