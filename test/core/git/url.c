#include "git.h"

typedef struct {
  const c8* name;
  const c8* url;
  const c8* expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "scp",
    .url = "git@github.com:A/B.git",
    .expect = "https://github.com/A/B.git"
  },
  {
    .name = "scp_no_suffix",
    .url = "git@github.com:A/B",
    .expect = "https://github.com/A/B"
  },
  {
    .name = "scp_deep_path",
    .url = "git@git.company.com:A/B/C/D.git",
    .expect = "https://git.company.com/A/B/C/D.git"
  },
  {
    .name = "scp_absolute_path",
    .url = "git@github.com:/A/B.git",
    .expect = "https://github.com/A/B.git"
  },
  {
    .name = "scp_host_case",
    .url = "git@GitHub.com:A/B.git",
    .expect = "https://github.com/A/B.git"
  },
  {
    .name = "ssh",
    .url = "ssh://git@github.com/A/B.git",
    .expect = "https://github.com/A/B.git"
  },
  {
    .name = "ssh_user",
    .url = "ssh://U@git.company.com/A/B.git",
    .expect = "https://git.company.com/A/B.git"
  },
  {
    .name = "ssh_no_user",
    .url = "ssh://github.com/A/B.git",
    .expect = "https://github.com/A/B.git"
  },
  {
    .name = "https",
    .url = "https://github.com/A/B.git"
  },
  {
    .name = "https_case",
    .url = "https://GitHub.com/A/B.git",
    .expect = "https://github.com/A/B.git"
  },
  {
    .name = "scheme_case",
    .url = "HTTPS://github.com/A/B.git",
    .expect = "https://github.com/A/B.git"
  },
  {
    .name = "http",
    .url = "http://git.company.com/A/B.git"
  },
  {
    .name = "git_scheme",
    .url = "git://github.com/A/B.git"
  },
  {
    .name = "local_path",
    .url = "/A/B"
  },
  {
    .name = "file",
    .url = "file:///A/B"
  },
  {
    .name = "ssh_port",
    .url = "ssh://git@github.com:2222/A/B.git"
  },
  {
    .name = "windows_drive",
    .url = "C:/A/B"
  },
  {
    .name = "no_path",
    .url = "git@github.com"
  },
  {
    .name = "empty",
    .url = ""
  },
};

sp_test_each(git_url, web, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t web = spn_git_url_web(mem, sp_cstr_as_str(it->url));
  sp_expect_str_eq_c(t, web, it->expect ? it->expect : it->url);
  return SP_OK;
}

sp_test_each(git_url, idempotent, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t web = spn_git_url_web(mem, sp_cstr_as_str(it->url));
  sp_expect_str_eq(t, spn_git_url_web(mem, web), web);
  return SP_OK;
}
