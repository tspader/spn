#include "lanes.h"

#include "codegen/toolchain.h"
#include "toml/loader.h"
#include "intern/intern.h"

lanes_read_t lanes_read(sp_mem_t mem, sp_str_t path, lanes_t* lanes) {
  *lanes = (lanes_t) { .mem = mem, .intern = sp_intern_new(mem), .path = path };
  if (sp_io_read_file(mem, path, &lanes->text)) {
    return LANES_READ_UNREADABLE;
  }
  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, mem, lanes->intern);
  bool parsed = spn_toolchains_parse(&loader, lanes->text, &lanes->config);
  lanes->issues = loader.issues;
  return parsed ? LANES_READ_OK : LANES_READ_PARSE;
}

static bool issue_in_entry(const spn_codegen_issue_t* issue, u32 at) {
  return issue->depth >= 2
    && issue->segs[0].kind == SPN_CODEGEN_PATH_KEY && sp_cstr_equal(issue->segs[0].key, "toolchain")
    && issue->segs[1].kind == SPN_CODEGEN_PATH_INDEX && issue->segs[1].index == at;
}

const spn_cg_toolchain_decl_t* lanes_find(const lanes_t* lanes, sp_str_t name) {
  sp_da_for(lanes->config.toolchain, it) {
    if (sp_str_equal(lanes->config.toolchain[it].name, name)) {
      return &lanes->config.toolchain[it];
    }
  }
  return SP_NULLPTR;
}

sp_da(spn_codegen_issue_t) lanes_lower(const lanes_t* lanes, u32 at, spn_path_root_t base, spn_toolchain_decl_t* decl) {
  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, lanes->mem, lanes->intern);
  sp_da_for(lanes->issues, it) {
    if (issue_in_entry(&lanes->issues[it], at)) {
      sp_da_push(loader.issues, lanes->issues[it]);
    }
  }
  *decl = spn_toolchain_lower(&loader, at, base, &lanes->config.toolchain[at]);
  return loader.issues;
}

static bool is_header(sp_str_t line) {
  return sp_str_equal_cstr(sp_str_trim(line), "[[toolchain]]");
}

sp_str_t lanes_text(const lanes_t* lanes, sp_str_t name) {
  sp_mem_t mem = lanes->mem;
  sp_str_t needle = sp_fmt(mem, "name = \"{}\"", sp_fmt_str(name)).value;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, lanes->text, '\n');

  s32 start = -1;
  bool found = false;
  sp_da_for(lines, it) {
    if (is_header(lines[it])) {
      if (found) {
        return sp_str_join_n(mem, lines + start, (u32)it - (u32)start, sp_str_lit("\n"));
      }
      start = (s32)it;
    }
    else if (start >= 0 && sp_str_equal(sp_str_trim(lines[it]), needle)) {
      found = true;
    }
  }
  if (found) {
    return sp_str_join_n(mem, lines + start, (u32)sp_da_size(lines) - (u32)start, sp_str_lit("\n"));
  }
  return sp_str_lit("");
}

const spn_cg_artifact_t* lane_artifact(const spn_cg_toolchain_decl_t* lane, sp_str_t host) {
  sp_da_for(lane->host, it) {
    if (sp_str_equal(lane->host[it].key, host)) {
      return &lane->host[it].value;
    }
  }
  return SP_NULLPTR;
}
