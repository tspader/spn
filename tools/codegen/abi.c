#include "codegen.h"

typedef enum {
  ABI_VAL_VOID = 0,
  ABI_VAL_S32,
  ABI_VAL_STR,
  ABI_VAL_STR_OPT,
  ABI_VAL_CTX,
  ABI_VAL_CONFIG,
  ABI_VAL_TARGET,
  ABI_VAL_NODE,
  ABI_VAL_NODE_CTX,
  ABI_VAL_PROFILE,
  ABI_VAL_MAKE,
  ABI_VAL_AUTOCONF,
  ABI_VAL_CMAKE,
} abi_val_t;

#define ABI_MAX_ARGS 6

typedef struct {
  const c8* name;
  const c8* host;
  bool abi;
  abi_val_t ret;
  abi_val_t args [ABI_MAX_ARGS];
} abi_fn_t;

static const abi_fn_t abi_fns [] = {
  { .name = "spn_get_target",           .ret = ABI_VAL_TARGET,  .args = { ABI_VAL_CTX, ABI_VAL_STR } },
  { .name = "spn_get_dep",              .ret = ABI_VAL_CTX,     .args = { ABI_VAL_CTX, ABI_VAL_STR } },
  { .name = "spn_get_dir",              .ret = ABI_VAL_STR,     .args = { ABI_VAL_CTX, ABI_VAL_S32 } },
  { .name = "spn_get_subdir",           .ret = ABI_VAL_STR,     .args = { ABI_VAL_CTX, ABI_VAL_S32, ABI_VAL_STR } },
  { .name = "spn_get_profile",          .ret = ABI_VAL_PROFILE, .args = { ABI_VAL_CTX } },
  { .name = "spn_profile_get_libc",     .ret = ABI_VAL_S32,     .args = { ABI_VAL_PROFILE } },
  { .name = "spn_profile_get_linkage",  .ret = ABI_VAL_S32,     .args = { ABI_VAL_PROFILE } },
  { .name = "spn_profile_get_standard", .ret = ABI_VAL_S32,     .args = { ABI_VAL_PROFILE } },
  { .name = "spn_profile_get_mode",     .ret = ABI_VAL_S32,     .args = { ABI_VAL_PROFILE } },
  { .name = "spn_add_exe",              .ret = ABI_VAL_TARGET,  .args = { ABI_VAL_CONFIG, ABI_VAL_STR } },
  { .name = "spn_add_lib",              .ret = ABI_VAL_TARGET,  .args = { ABI_VAL_CONFIG, ABI_VAL_STR, ABI_VAL_S32 } },
  { .name = "spn_add_test",             .ret = ABI_VAL_TARGET,  .args = { ABI_VAL_CONFIG, ABI_VAL_STR } },
  { .name = "spn_add_include",          .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CONFIG, ABI_VAL_STR } },
  { .name = "spn_add_define",           .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CONFIG, ABI_VAL_STR } },
  { .name = "spn_add_system_dep",       .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CONFIG, ABI_VAL_STR } },
  { .name = "spn_log",                  .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CTX, ABI_VAL_STR } },
  { .name = "spn_target_add_source",    .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR } },
  { .name = "spn_target_add_include",   .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR } },
  { .name = "spn_target_add_define",    .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR } },
  { .name = "spn_target_add_flag",      .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR } },
  { .name = "spn_target_set_linked",    .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_S32 } },
  { .name = "spn_target_embed_file",    .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR } },
  { .name = "spn_target_embed_file_ex", .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR, ABI_VAL_STR, ABI_VAL_STR, ABI_VAL_STR } },
  { .name = "spn_target_embed_dir",     .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR } },
  { .name = "spn_target_embed_dir_ex",  .ret = ABI_VAL_VOID,    .args = { ABI_VAL_TARGET, ABI_VAL_STR, ABI_VAL_STR, ABI_VAL_STR } },
  { .name = "spn_add_node",             .ret = ABI_VAL_NODE,    .args = { ABI_VAL_CONFIG, ABI_VAL_STR } },
  { .name = "spn_node_set_fn",          .ret = ABI_VAL_VOID,    .args = { ABI_VAL_NODE, ABI_VAL_STR } },
  { .name = "spn_node_set_user_data",   .ret = ABI_VAL_VOID,    .args = { ABI_VAL_NODE, ABI_VAL_S32 }, .host = "spn_abi_node_set_user_data" },
  { .name = "spn_node_add_input",       .ret = ABI_VAL_VOID,    .args = { ABI_VAL_NODE, ABI_VAL_STR } },
  { .name = "spn_node_add_output",      .ret = ABI_VAL_VOID,    .args = { ABI_VAL_NODE, ABI_VAL_STR } },
  { .name = "spn_node_link",            .ret = ABI_VAL_VOID,    .args = { ABI_VAL_NODE, ABI_VAL_NODE } },
  { .name = "spn_node_ctx_get_user_data", .ret = ABI_VAL_S32,   .args = { ABI_VAL_NODE_CTX }, .host = "spn_abi_node_ctx_get_user_data" },
  { .name = "spn_write_file",           .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CTX, ABI_VAL_STR, ABI_VAL_STR } },
  { .name = "spn_copy",                 .ret = ABI_VAL_S32,     .args = { ABI_VAL_CTX, ABI_VAL_S32, ABI_VAL_STR, ABI_VAL_S32, ABI_VAL_STR } },
  { .name = "spn_fs_copy",              .ret = ABI_VAL_VOID,    .args = { ABI_VAL_STR, ABI_VAL_STR }, .host = "spn_abi_fs_copy", .abi = true },
  { .name = "spn_fs_copy_glob",         .ret = ABI_VAL_VOID,    .args = { ABI_VAL_STR, ABI_VAL_STR }, .host = "spn_abi_fs_copy_glob", .abi = true },
  { .name = "spn_fs_cat_ex",            .ret = ABI_VAL_VOID,    .args = { ABI_VAL_STR, ABI_VAL_STR_OPT, ABI_VAL_STR_OPT, ABI_VAL_STR_OPT, ABI_VAL_STR_OPT }, .host = "spn_abi_fs_cat", .abi = true },
  { .name = "spn_fs_create_dir",        .ret = ABI_VAL_VOID,    .args = { ABI_VAL_STR }, .host = "spn_abi_fs_create_dir", .abi = true },
  { .name = "spn_io_write",             .ret = ABI_VAL_VOID,    .args = { ABI_VAL_STR, ABI_VAL_STR }, .host = "spn_abi_io_write", .abi = true },
  { .name = "spn_fmt_ex",               .ret = ABI_VAL_STR,     .args = { ABI_VAL_STR, ABI_VAL_STR_OPT, ABI_VAL_STR_OPT, ABI_VAL_STR_OPT, ABI_VAL_STR_OPT }, .host = "spn_abi_fmt", .abi = true },
  { .name = "spn_make",                 .ret = ABI_VAL_S32,     .args = { ABI_VAL_CTX } },
  { .name = "spn_make_new",             .ret = ABI_VAL_MAKE,    .args = { ABI_VAL_CTX } },
  { .name = "spn_make_add_target",      .ret = ABI_VAL_VOID,    .args = { ABI_VAL_MAKE, ABI_VAL_STR } },
  { .name = "spn_make_run",             .ret = ABI_VAL_S32,     .args = { ABI_VAL_MAKE } },
  { .name = "spn_autoconf",             .ret = ABI_VAL_S32,     .args = { ABI_VAL_CTX } },
  { .name = "spn_autoconf_new",         .ret = ABI_VAL_AUTOCONF, .args = { ABI_VAL_CTX } },
  { .name = "spn_autoconf_add_flag",    .ret = ABI_VAL_VOID,    .args = { ABI_VAL_AUTOCONF, ABI_VAL_STR } },
  { .name = "spn_autoconf_run",         .ret = ABI_VAL_S32,     .args = { ABI_VAL_AUTOCONF } },
  { .name = "spn_cmake",                .ret = ABI_VAL_S32,     .args = { ABI_VAL_CTX } },
  { .name = "spn_cmake_new",            .ret = ABI_VAL_CMAKE,   .args = { ABI_VAL_CTX } },
  { .name = "spn_cmake_set_generator",  .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CMAKE, ABI_VAL_S32 } },
  { .name = "spn_cmake_add_define",     .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CMAKE, ABI_VAL_STR, ABI_VAL_STR } },
  { .name = "spn_cmake_add_arg",        .ret = ABI_VAL_VOID,    .args = { ABI_VAL_CMAKE, ABI_VAL_STR } },
  { .name = "spn_cmake_configure",      .ret = ABI_VAL_S32,     .args = { ABI_VAL_CMAKE } },
  { .name = "spn_cmake_build",          .ret = ABI_VAL_S32,     .args = { ABI_VAL_CMAKE } },
  { .name = "spn_cmake_install",        .ret = ABI_VAL_S32,     .args = { ABI_VAL_CMAKE } },
  { .name = "spn_cmake_run",            .ret = ABI_VAL_S32,     .args = { ABI_VAL_CMAKE } },
};

static bool abi_val_is_handle(abi_val_t val) {
  return val >= ABI_VAL_CTX;
}

static const c8* abi_val_kind(abi_val_t val) {
  switch (val) {
    case ABI_VAL_CTX:      return "SPN_ABI_KIND_CTX";
    case ABI_VAL_CONFIG:   return "SPN_ABI_KIND_CONFIG";
    case ABI_VAL_TARGET:   return "SPN_ABI_KIND_TARGET";
    case ABI_VAL_NODE:     return "SPN_ABI_KIND_NODE";
    case ABI_VAL_NODE_CTX: return "SPN_ABI_KIND_NODE_CTX";
    case ABI_VAL_PROFILE:  return "SPN_ABI_KIND_PROFILE";
    case ABI_VAL_MAKE:     return "SPN_ABI_KIND_MAKE";
    case ABI_VAL_AUTOCONF: return "SPN_ABI_KIND_AUTOCONF";
    case ABI_VAL_CMAKE:    return "SPN_ABI_KIND_CMAKE";
    default:               return "";
  }
}

static u32 abi_fn_num_args(const abi_fn_t* fn) {
  u32 count = 0;
  while (count < ABI_MAX_ARGS && fn->args[count] != ABI_VAL_VOID) count++;
  return count;
}

static sp_str_t abi_fn_sig(gen_t* g, const abi_fn_t* fn) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(g->mem, &w);

  sp_fmt_io(&w.base, "(");
  sp_for(it, abi_fn_num_args(fn)) {
    sp_fmt_io(&w.base, "{}", sp_fmt_cstr(fn->args[it] == ABI_VAL_STR ? "$" : "i"));
  }
  sp_fmt_io(&w.base, ")");
  if (fn->ret != ABI_VAL_VOID) {
    sp_fmt_io(&w.base, "i");
  }
  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t abi_fn_thunk(gen_t* g, const abi_fn_t* fn) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(g->mem, &w);
  sp_io_writer_t* io = &w.base;

  const c8* host = fn->host ? fn->host : fn->name;
  u32 num_args = abi_fn_num_args(fn);

  const c8* ret_type = "u32";
  const c8* fail = "return 0;";
  switch (fn->ret) {
    case ABI_VAL_VOID: ret_type = "void"; fail = "return;"; break;
    case ABI_VAL_S32:  ret_type = "s32"; fail = "return SPN_ERROR;"; break;
    default: break;
  }

  sp_fmt_io(io, "static {} spn_abi_thunk_{}(wasm_exec_env_t env", sp_fmt_cstr(ret_type), sp_fmt_cstr(fn->name + 4));
  sp_for(it, num_args) {
    const c8* type = "u32";
    switch (fn->args[it]) {
      case ABI_VAL_STR: type = "const c8*"; break;
      case ABI_VAL_S32: type = "s32"; break;
      default: break;
    }
    sp_fmt_io(io, ", {} a{}", sp_fmt_cstr(type), sp_fmt_uint(it));
  }
  sp_fmt_io(io, ") {{\n");
  sp_fmt_io(io, "  spn_wasm_ctx_t abi = spn_wasm_ctx(env);\n");

  sp_for(it, num_args) {
    if (fn->args[it] == ABI_VAL_STR_OPT) {
      sp_fmt_io(io, "  const c8* s{} = SP_NULLPTR;\n", sp_fmt_uint(it));
      sp_fmt_io(io, "  if (!spn_wasm_read_str(&abi, a{}, &s{})) {}\n", sp_fmt_uint(it), sp_fmt_uint(it), sp_fmt_cstr(fail));
      continue;
    }
    if (!abi_val_is_handle(fn->args[it])) continue;
    sp_fmt_io(io, "  void* h{} = spn_wasm_resolve_handle(abi.handles, a{}, {});\n", sp_fmt_uint(it), sp_fmt_uint(it), sp_fmt_cstr(abi_val_kind(fn->args[it])));
    sp_fmt_io(io, "  if (!h{}) {}\n", sp_fmt_uint(it), sp_fmt_cstr(fail));
  }

  sp_io_dyn_mem_writer_t args = sp_zero;
  sp_io_dyn_mem_writer_init(g->mem, &args);
  if (fn->abi) {
    sp_fmt_io(&args.base, "&abi");
  }
  sp_for(it, num_args) {
    const c8* prefix = "a";
    if (abi_val_is_handle(fn->args[it])) prefix = "h";
    if (fn->args[it] == ABI_VAL_STR_OPT) prefix = "s";
    sp_fmt_io(&args.base, "{}{}{}",
      sp_fmt_cstr((it || fn->abi) ? ", " : ""),
      sp_fmt_cstr(prefix),
      sp_fmt_uint(it));
  }
  sp_str_t call = sp_io_dyn_mem_writer_take_str(&args);

  switch (fn->ret) {
    case ABI_VAL_VOID: {
      sp_fmt_io(io, "  {}({});\n", sp_fmt_cstr(host), sp_fmt_str(call));
      break;
    }
    case ABI_VAL_S32: {
      sp_fmt_io(io, "  return {}({});\n", sp_fmt_cstr(host), sp_fmt_str(call));
      break;
    }
    case ABI_VAL_STR: {
      sp_fmt_io(io, "  return spn_wasm_copy_str(&abi, {}({}));\n", sp_fmt_cstr(host), sp_fmt_str(call));
      break;
    }
    default: {
      sp_fmt_io(io, "  void* result = (void*)({}({}));\n", sp_fmt_cstr(host), sp_fmt_str(call));
      sp_fmt_io(io, "  if (!result) return 0;\n");
      sp_fmt_io(io, "  return spn_wasm_add_handle(abi.handles, result, {});\n", sp_fmt_cstr(abi_val_kind(fn->ret)));
      break;
    }
  }

  sp_fmt_io(io, "}}\n");
  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t abi_struct_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t abi_wire_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_wire_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t abi_handle_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_{}_t*", sp_fmt_str(name)).value;
}

static bool abi_check(gen_t* g, type_t* type) {
  sp_da_for(type->fields, it) {
    field_t* field = &type->fields[it];
    bool ok = false;
    if (field->card == CARD_SCALAR) {
      ok = field->kind == FIELD_STR || field->kind == FIELD_HANDLE || field->kind == FIELD_STRUCT;
    }
    if (field->card == CARD_ARRAY) {
      ok = field->kind == FIELD_STR && field->cap > 0;
    }
    if (!ok) {
      g->err = sp_fmt(g->mem, "{.cyan}.{.cyan}: abi supports strings, handles, nested structs, and capped string arrays",
        sp_fmt_str(type->name), sp_fmt_str(field->key)).value;
      return false;
    }
  }
  return true;
}

static u32 abi_type_size(gen_t* g, type_t* type);
static u32 abi_type_align(gen_t* g, type_t* type);

static u32 abi_field_size(gen_t* g, field_t* field) {
  if (field->card == CARD_ARRAY) {
    return 4 * field->cap;
  }
  if (field->kind == FIELD_STRUCT) {
    return abi_type_size(g, gen_type(g, field->type_name));
  }
  return 4;
}

static u32 abi_field_align(gen_t* g, field_t* field) {
  if (field->kind == FIELD_STRUCT && field->card == CARD_SCALAR) {
    return abi_type_align(g, gen_type(g, field->type_name));
  }
  return 4;
}

static u32 abi_align_up(u32 value, u32 align) {
  return (value + align - 1) & ~(align - 1);
}

static u32 abi_type_align(gen_t* g, type_t* type) {
  u32 align = 1;
  sp_da_for(type->fields, it) {
    u32 field_align = abi_field_align(g, &type->fields[it]);
    if (field_align > align) align = field_align;
  }
  return align;
}

static u32 abi_type_size(gen_t* g, type_t* type) {
  u32 size = 0;
  sp_da_for(type->fields, it) {
    field_t* field = &type->fields[it];
    size = abi_align_up(size, abi_field_align(g, field));
    size += abi_field_size(g, field);
  }
  return abi_align_up(size, abi_type_align(g, type));
}

static sp_str_t abi_field_decl(gen_t* g, field_t* field, bool wire) {
  sp_str_t type = sp_zero;
  switch (field->kind) {
    case FIELD_STR:    type = wire ? sp_str_lit("u32") : sp_str_lit("const c8*"); break;
    case FIELD_HANDLE: type = wire ? sp_str_lit("u32") : abi_handle_type(g, field->type_name); break;
    case FIELD_STRUCT: type = wire ? abi_wire_type(g, field->type_name) : abi_struct_type(g, field->type_name); break;
    case FIELD_BOOL:
    case FIELD_ENUM: break;
  }

  if (field->card == CARD_ARRAY) {
    return sp_fmt(g->mem, "{} {} [{}]", sp_fmt_str(type), sp_fmt_str(field->key), sp_fmt_uint(field->cap)).value;
  }
  return sp_fmt(g->mem, "{} {}", sp_fmt_str(type), sp_fmt_str(field->key)).value;
}

static void abi_bind_field(gen_t* g, sp_template_scope_t* scope, field_t* field) {
  sp_template_set(scope, sp_str_lit("key"), field->key);
  sp_template_set(scope, sp_str_lit("decl"), abi_field_decl(g, field, false));
  sp_template_set(scope, sp_str_lit("wire_decl"), abi_field_decl(g, field, true));

  if (field->card == CARD_ARRAY) {
    sp_template_set(scope, sp_str_lit("cap"), sp_fmt(g->mem, "{}", sp_fmt_uint(field->cap)).value);
    sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/str_array"));
    return;
  }
  switch (field->kind) {
    case FIELD_STR: {
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/str"));
      break;
    }
    case FIELD_HANDLE: {
      sp_str_t kind = sp_fmt(g->mem, "SPN_ABI_KIND_{}", sp_fmt_str(sp_str_to_upper(g->mem, field->type_name))).value;
      sp_template_set(scope, sp_str_lit("kind"), kind);
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/handle"));
      break;
    }
    case FIELD_STRUCT: {
      sp_template_set(scope, sp_str_lit("object"), field->type_name);
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/object"));
      break;
    }
    case FIELD_BOOL:
    case FIELD_ENUM: break;
  }
}

static sp_template_scope_t* abi_scope(gen_t* g) {
  sp_template_scope_t* root = sp_template_scope_create(g->mem);

  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("types"));
    sp_template_set(scope, sp_str_lit("name"), type->name);
    sp_template_set(scope, sp_str_lit("type"), abi_struct_type(g, type->name));
    sp_template_set(scope, sp_str_lit("wire_type"), abi_wire_type(g, type->name));
    sp_template_set(scope, sp_str_lit("size"), sp_fmt(g->mem, "{}", sp_fmt_uint(abi_type_size(g, type))).value);
    sp_da_for(type->fields, field) {
      abi_bind_field(g, sp_template_push(scope, sp_str_lit("fields")), &type->fields[field]);
    }
  }

  sp_da_for(g->roots, it) {
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("functions"));
    sp_template_set(scope, sp_str_lit("name"), g->roots[it]);
    sp_template_set(scope, sp_str_lit("type"), abi_struct_type(g, g->roots[it]));
  }

  sp_carr_for(abi_fns, it) {
    const abi_fn_t* fn = &abi_fns[it];
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("fns"));
    sp_template_set(scope, sp_str_lit("name"), sp_cstr_as_str(fn->name));
    sp_template_set(scope, sp_str_lit("short"), sp_cstr_as_str(fn->name + 4));
    sp_template_set(scope, sp_str_lit("sig"), abi_fn_sig(g, fn));
    sp_template_set(scope, sp_str_lit("thunk"), abi_fn_thunk(g, fn));
  }

  return root;
}

bool render_abi_decls(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg) {
  sp_om_for(g->types, it) {
    if (!abi_check(g, sp_str_om_at(g->types, it))) {
      return false;
    }
  }
  return gen_render(g, out, reg, "decls", abi_scope(g));
}

bool render_abi_impl(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg) {
  return gen_render(g, out, reg, "impl", abi_scope(g));
}
