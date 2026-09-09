#include "lanes.h"

#include "codegen/toolchain.h"
#include "toml/loader.h"
#include "intern/intern.h"

bool lanes_read(sp_mem_t mem, sp_str_t path, lanes_t* out, sp_str_t* issues) {
  *out = (lanes_t) { .mem = mem, .intern = sp_intern_new(mem), .path = path };
  *issues = sp_str_lit("");
  if (sp_io_read_file(mem, path, &out->text)) {
    *issues = sp_str_lit("unreadable");
    return false;
  }
  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, mem, out->intern);
  toml_table_t* table = spn_codegen_parse_str(&loader, out->text);
  if (!table) {
    *issues = spn_codegen_issues_message(mem, loader.issues);
    return false;
  }
  spn_config_read(&loader, table, &out->config);
  toml_free(table);
  out->issues = loader.issues;
  return true;
}

// The reader records an issue under the entry's path, `toolchain[N]...`.
static bool issue_in_entry(const spn_codegen_issue_t* issue, u32 at) {
  sp_str_t prefix = sp_str_lit("toolchain[");
  if (!sp_str_starts_with(issue->path, prefix)) {
    return false;
  }
  sp_str_t rest = sp_str_suffix(issue->path, issue->path.len - prefix.len);
  s32 close = sp_str_find_c8(rest, ']');
  return close > 0 && sp_parse_u64(sp_str_prefix(rest, (u32)close)) == at;
}

const spn_cg_toolchain_decl_t* lanes_find(const lanes_t* lanes, sp_str_t name) {
  sp_da_for(lanes->config.toolchain, it) {
    if (sp_str_equal(lanes->config.toolchain[it].name, name)) {
      return &lanes->config.toolchain[it];
    }
  }
  return SP_NULLPTR;
}

sp_str_t lanes_lower(const lanes_t* lanes, u32 at, spn_path_root_t base, spn_toolchain_decl_t* out) {
  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, lanes->mem, lanes->intern);
  sp_da_for(lanes->issues, it) {
    if (issue_in_entry(&lanes->issues[it], at)) {
      sp_da_push(loader.issues, lanes->issues[it]);
    }
  }
  *out = spn_toolchain_lower(&loader, at, base, &lanes->config.toolchain[at]);
  return sp_da_empty(loader.issues) ? sp_str_lit("") : spn_codegen_issues_message(lanes->mem, loader.issues);
}

// Blocks start at a line that is exactly the array header; a block owns
// everything up to the next one, its [toolchain.host] subtable included.
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
