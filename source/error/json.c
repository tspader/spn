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

void spn_codegen_write_pkg_name(sp_io_writer_t* out, const spn_pkg_name_t* id) {
  bool first = true;
  sp_io_write_c8(out, '{');
  spn_codegen_json_key(out, &first, sp_str_lit("namespace"));
  spn_codegen_json_str(out, id->namespace);
  spn_codegen_json_key(out, &first, sp_str_lit("name"));
  spn_codegen_json_str(out, id->name);
  sp_io_write_c8(out, '}');
}

void spn_codegen_write_requested_dep(sp_io_writer_t* out, const spn_requested_dep_t* request) {
  bool first = true;
  sp_io_write_c8(out, '{');
  spn_codegen_json_key(out, &first, sp_str_lit("qualified"));
  spn_codegen_json_str(out, request->qualified);
  sp_io_write_c8(out, '}');
}

void spn_codegen_write_triple(sp_io_writer_t* out, const spn_triple_t* triple) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_codegen_json_str(out, spn_triple_to_str(scratch.mem, *triple));
  sp_mem_end_scratch(scratch);
}

void spn_codegen_write_sanitizer_set(sp_io_writer_t* out, const spn_sanitizer_set_t* set) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_codegen_json_str(out, spn_sanitizer_set_to_str(scratch.mem, *set));
  sp_mem_end_scratch(scratch);
}

static void write_option_value(sp_io_writer_t* out, spn_option_value_t value) {
  switch (value.kind) {
    case SPN_OPTION_VALUE_NONE: {
      sp_io_write_cstr(out, "null", SP_NULLPTR);
      break;
    }
    case SPN_OPTION_VALUE_BOOL: {
      spn_codegen_json_bool(out, value.b);
      break;
    }
    case SPN_OPTION_VALUE_STR: {
      spn_codegen_json_str(out, value.str);
      break;
    }
  }
}

static void write_option_setter(sp_io_writer_t* out, spn_option_setter_t setter) {
  bool first = true;
  sp_io_write_c8(out, '{');
  spn_codegen_json_key(out, &first, sp_str_lit("kind"));
  spn_codegen_json_str(out, spn_option_setter_kind_to_str(setter.kind));
  spn_codegen_json_key(out, &first, sp_str_lit("name"));
  spn_codegen_json_str(out, setter.name);
  sp_io_write_c8(out, '}');
}

void spn_codegen_write_option_violation(sp_io_writer_t* out, const spn_option_violation_t* violation) {
  bool first = true;
  sp_io_write_c8(out, '{');
  spn_codegen_json_key(out, &first, sp_str_lit("kind"));
  spn_codegen_json_str(out, spn_option_err_to_str(violation->kind));
  spn_codegen_json_key(out, &first, sp_str_lit("pkg"));
  spn_codegen_json_str(out, violation->pkg);
  spn_codegen_json_key(out, &first, sp_str_lit("option"));
  spn_codegen_json_str(out, violation->option);
  spn_codegen_json_key(out, &first, sp_str_lit("value"));
  write_option_value(out, violation->value);
  spn_codegen_json_key(out, &first, sp_str_lit("a"));
  write_option_setter(out, violation->a);
  spn_codegen_json_key(out, &first, sp_str_lit("b"));
  write_option_setter(out, violation->b);
  sp_io_write_c8(out, '}');
}
