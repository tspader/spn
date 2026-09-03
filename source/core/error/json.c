#include "error/json.h"

#include "codegen/codegen.h"
#include "enum/enum.h"
#include "triple/triple.h"

void spn_codegen_write_err(sp_io_writer_t* out, const spn_err_t* err) {
  spn_codegen_json_str(out, spn_err_to_str(*err));
}

void spn_codegen_write_semver(sp_io_writer_t* out, const spn_semver_t* version) {
  bool first = true;
  sp_io_write_c8(out, '{');
  spn_codegen_json_key(out, &first, sp_str_lit("major"));
  spn_codegen_json_u64(out, version->major);
  spn_codegen_json_key(out, &first, sp_str_lit("minor"));
  spn_codegen_json_u64(out, version->minor);
  spn_codegen_json_key(out, &first, sp_str_lit("patch"));
  spn_codegen_json_u64(out, version->patch);
  sp_io_write_c8(out, '}');
}

bool spn_codegen_semver_present(const spn_semver_t* version) {
  return version->major || version->minor || version->patch;
}

void spn_codegen_write_triple(sp_io_writer_t* out, const spn_triple_t* triple) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_codegen_json_str(out, spn_triple_to_str(scratch.mem, *triple));
  sp_mem_end_scratch(scratch);
}

void spn_codegen_write_abi(sp_io_writer_t* out, const spn_abi_t* abi) {
  spn_codegen_json_str(out, spn_abi_to_str(*abi));
}

void spn_codegen_write_sanitizer_set(sp_io_writer_t* out, const spn_sanitizer_set_t* set) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_codegen_json_str(out, spn_sanitizer_set_to_str(scratch.mem, *set));
  sp_mem_end_scratch(scratch);
}
