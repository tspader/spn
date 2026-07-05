#include "codegen.h"

static void* yj_malloc(void* ctx, size_t size) {
  return sp_alloc(*(sp_mem_t*)ctx, size);
}

static void* yj_realloc(void* ctx, void* ptr, size_t old_size, size_t size) {
  return sp_realloc(*(sp_mem_t*)ctx, ptr, old_size, size);
}

static void yj_free(void* ctx, void* ptr) {
}

yyjson_alc cg_yyjson_alc(sp_mem_t* mem) {
  return (yyjson_alc) {
    .malloc = yj_malloc,
    .realloc = yj_realloc,
    .free = yj_free,
    .ctx = mem,
  };
}

static bool abi_fail(abi_t* abi, sp_str_t where, const c8* what) {
  abi->err = sp_fmt(abi->mem, "abi: {.cyan}: {}", sp_fmt_str(where), sp_fmt_cstr(what)).value;
  return false;
}

static sp_str_t abi_str(abi_t* abi, yyjson_val* val) {
  return sp_str_from_cstr_n(abi->mem, yyjson_get_str(val), (u32)yyjson_get_len(val));
}

static bool abi_is_handle(abi_t* abi, sp_str_t name) {
  return sp_ht_getp(abi->handles, name) != SP_NULLPTR;
}

static abi_type_t* abi_type(abi_t* abi, sp_str_t name) {
  if (!sp_str_om_has(abi->types, name)) {
    return SP_NULLPTR;
  }
  return sp_str_om_get(abi->types, name);
}

static bool abi_parse_val(abi_t* abi, sp_str_t where, yyjson_val* val, bool structs, abi_val_t* parsed) {
  if (!yyjson_is_str(val)) {
    return abi_fail(abi, where, "expected a string");
  }
  sp_str_t name = abi_str(abi, val);
  if (sp_str_equal_cstr(name, "s32")) {
    *parsed = (abi_val_t) { .kind = ABI_VAL_S32 };
  }
  else if (sp_str_equal_cstr(name, "str")) {
    *parsed = (abi_val_t) { .kind = ABI_VAL_STR };
  }
  else if (sp_str_equal_cstr(name, "str_opt")) {
    *parsed = (abi_val_t) { .kind = ABI_VAL_STR_OPT };
  }
  else if (abi_is_handle(abi, name)) {
    *parsed = (abi_val_t) { .kind = ABI_VAL_HANDLE, .name = name };
  }
  else if (structs && abi_type(abi, name)) {
    *parsed = (abi_val_t) { .kind = ABI_VAL_STRUCT, .name = name };
  }
  else {
    abi->err = sp_fmt(abi->mem, "abi: {.cyan}: unknown type {.red}", sp_fmt_str(where), sp_fmt_str(name)).value;
    return false;
  }
  return true;
}

static bool abi_parse_field(abi_t* abi, abi_type_t* type, yyjson_val* val) {
  if (!yyjson_is_obj(val)) {
    return abi_fail(abi, type->name, "field must be an object");
  }

  abi_field_t field = sp_zero;
  yyjson_val* field_type = SP_NULLPTR;

  size_t idx, max;
  yyjson_val *k, *v;
  yyjson_obj_foreach(val, idx, max, k, v) {
    sp_str_t key = abi_str(abi, k);
    if (sp_str_equal_cstr(key, "name")) {
      if (!yyjson_is_str(v)) {
        return abi_fail(abi, type->name, "field name must be a string");
      }
      field.name = abi_str(abi, v);
    }
    else if (sp_str_equal_cstr(key, "type")) {
      field_type = v;
    }
    else if (sp_str_equal_cstr(key, "cap")) {
      if (!yyjson_is_uint(v) || !yyjson_get_uint(v)) {
        return abi_fail(abi, type->name, "cap must be a positive integer");
      }
      field.cap = (u32)yyjson_get_uint(v);
    }
    else {
      abi->err = sp_fmt(abi->mem, "abi: {.cyan}: unknown field key {.red}", sp_fmt_str(type->name), sp_fmt_str(key)).value;
      return false;
    }
  }

  sp_str_t where = sp_fmt(abi->mem, "{}.{}", sp_fmt_str(type->name), sp_fmt_str(field.name)).value;
  if (sp_str_empty(field.name) || !field_type) {
    return abi_fail(abi, type->name, "field needs a name and a type");
  }
  if (!abi_parse_val(abi, where, field_type, true, &field.val)) {
    return false;
  }
  if (field.val.kind != ABI_VAL_STR && field.val.kind != ABI_VAL_HANDLE && field.val.kind != ABI_VAL_STRUCT) {
    return abi_fail(abi, where, "abi supports string, handle, and struct fields");
  }
  if (field.cap && field.val.kind != ABI_VAL_STR) {
    return abi_fail(abi, where, "only string fields can have a cap");
  }

  sp_da_push(type->fields, field);
  return true;
}

static bool abi_parse_type(abi_t* abi, yyjson_val* val) {
  if (!yyjson_is_obj(val)) {
    return abi_fail(abi, sp_str_lit("types"), "type must be an object");
  }

  abi_type_t type = {
    .fields = sp_da_new(abi->mem, abi_field_t),
  };
  yyjson_val* fields = SP_NULLPTR;

  size_t idx, max;
  yyjson_val *k, *v;
  yyjson_obj_foreach(val, idx, max, k, v) {
    sp_str_t key = abi_str(abi, k);
    if (sp_str_equal_cstr(key, "name")) {
      if (!yyjson_is_str(v)) {
        return abi_fail(abi, sp_str_lit("types"), "type name must be a string");
      }
      type.name = abi_str(abi, v);
    }
    else if (sp_str_equal_cstr(key, "fn")) {
      if (!yyjson_is_bool(v)) {
        return abi_fail(abi, type.name, "fn must be a boolean");
      }
      type.fn = yyjson_get_bool(v);
    }
    else if (sp_str_equal_cstr(key, "fields")) {
      if (!yyjson_is_arr(v)) {
        return abi_fail(abi, type.name, "fields must be an array");
      }
      fields = v;
    }
    else {
      abi->err = sp_fmt(abi->mem, "abi: unknown type key {.red}", sp_fmt_str(key)).value;
      return false;
    }
  }

  if (sp_str_empty(type.name) || !fields) {
    return abi_fail(abi, sp_str_lit("types"), "type needs a name and fields");
  }
  if (abi_type(abi, type.name) || abi_is_handle(abi, type.name)) {
    return abi_fail(abi, type.name, "duplicate type name");
  }

  yyjson_val* field;
  yyjson_arr_foreach(fields, idx, max, field) {
    if (!abi_parse_field(abi, &type, field)) {
      return false;
    }
  }

  sp_str_om_insert(abi->types, type.name, type);
  return true;
}

static bool abi_parse_export(abi_t* abi, yyjson_val* val) {
  if (!yyjson_is_obj(val)) {
    return abi_fail(abi, sp_str_lit("exports"), "export must be an object");
  }

  abi_export_t export = {
    .args = sp_da_new(abi->mem, abi_val_t),
  };
  yyjson_val* ret = SP_NULLPTR;
  yyjson_val* args = SP_NULLPTR;

  size_t idx, max;
  yyjson_val *k, *v;
  yyjson_obj_foreach(val, idx, max, k, v) {
    sp_str_t key = abi_str(abi, k);
    if (sp_str_equal_cstr(key, "name")) {
      if (!yyjson_is_str(v)) {
        return abi_fail(abi, sp_str_lit("exports"), "export name must be a string");
      }
      export.name = abi_str(abi, v);
    }
    else if (sp_str_equal_cstr(key, "host")) {
      if (!yyjson_is_str(v)) {
        return abi_fail(abi, export.name, "host must be a string");
      }
      export.host = abi_str(abi, v);
    }
    else if (sp_str_equal_cstr(key, "wasm_ctx")) {
      if (!yyjson_is_bool(v)) {
        return abi_fail(abi, export.name, "wasm_ctx must be a boolean");
      }
      export.wasm_ctx = yyjson_get_bool(v);
    }
    else if (sp_str_equal_cstr(key, "ret")) {
      ret = v;
    }
    else if (sp_str_equal_cstr(key, "args")) {
      if (!yyjson_is_arr(v)) {
        return abi_fail(abi, export.name, "args must be an array");
      }
      args = v;
    }
    else {
      abi->err = sp_fmt(abi->mem, "abi: unknown export key {.red}", sp_fmt_str(key)).value;
      return false;
    }
  }

  if (sp_str_empty(export.name)) {
    return abi_fail(abi, sp_str_lit("exports"), "export needs a name");
  }
  if (!sp_str_starts_with(export.name, sp_str_lit("spn_"))) {
    return abi_fail(abi, export.name, "export name must start with spn_");
  }
  sp_da_for(abi->exports, it) {
    if (sp_str_equal(abi->exports[it].name, export.name)) {
      return abi_fail(abi, export.name, "duplicate export name");
    }
  }
  if (ret && !abi_parse_val(abi, export.name, ret, false, &export.ret)) {
    return false;
  }
  if (export.ret.kind == ABI_VAL_STR_OPT) {
    return abi_fail(abi, export.name, "export cannot return str_opt");
  }
  if (args) {
    yyjson_val* arg;
    yyjson_arr_foreach(args, idx, max, arg) {
      abi_val_t parsed = sp_zero;
      if (!abi_parse_val(abi, export.name, arg, false, &parsed)) {
        return false;
      }
      sp_da_push(export.args, parsed);
    }
  }

  sp_da_push(abi->exports, export);
  return true;
}

abi_t* abi_parse(sp_mem_t mem, sp_str_t path) {
  abi_t* abi = sp_alloc_type(mem, abi_t);
  abi->mem = mem;
  sp_str_ht_init(mem, abi->handles);
  sp_str_om_init(abi->types);
  abi->exports = sp_da_new(mem, abi_export_t);

  sp_str_t json = sp_zero;
  if (sp_io_read_file(mem, path, &json)) {
    abi_fail(abi, path, "failed to read");
    return abi;
  }

  yyjson_alc alc = cg_yyjson_alc(&abi->mem);
  yyjson_doc* doc = yyjson_read_opts((c8*)json.data, json.len, 0, &alc, SP_NULLPTR);
  if (!doc) {
    abi_fail(abi, path, "invalid json");
    return abi;
  }

  yyjson_val* root = yyjson_doc_get_root(doc);
  yyjson_val* handles = yyjson_obj_get(root, "handles");
  yyjson_val* types = yyjson_obj_get(root, "types");
  yyjson_val* exports = yyjson_obj_get(root, "exports");
  if (!yyjson_is_arr(handles) || !yyjson_is_arr(types) || !yyjson_is_arr(exports) || yyjson_obj_size(root) != 3) {
    abi_fail(abi, path, "abi needs exactly handles, types, and exports arrays");
    return abi;
  }

  size_t idx, max;
  yyjson_val* val;
  yyjson_arr_foreach(handles, idx, max, val) {
    if (!yyjson_is_str(val)) {
      abi_fail(abi, sp_str_lit("handles"), "handle must be a string");
      break;
    }
    sp_str_t handle = abi_str(abi, val);
    if (abi_is_handle(abi, handle)) {
      abi_fail(abi, handle, "duplicate handle");
      break;
    }
    sp_ht_insert(abi->handles, handle, true);
  }
  if (sp_str_empty(abi->err)) {
    yyjson_arr_foreach(types, idx, max, val) {
      if (!abi_parse_type(abi, val)) {
        break;
      }
    }
  }
  if (sp_str_empty(abi->err)) {
    yyjson_arr_foreach(exports, idx, max, val) {
      if (!abi_parse_export(abi, val)) {
        break;
      }
    }
  }

  return abi;
}

static sp_str_t abi_struct_type(abi_t* abi, sp_str_t name) {
  return sp_fmt(abi->mem, "spn_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t abi_wire_type(abi_t* abi, sp_str_t name) {
  return sp_fmt(abi->mem, "spn_wire_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t abi_val_kind(abi_t* abi, abi_val_t val) {
  return sp_fmt(abi->mem, "SPN_ABI_KIND_{}", sp_fmt_str(sp_str_to_upper(abi->mem, val.name))).value;
}

static u32 abi_type_size(abi_t* abi, abi_type_t* type);
static u32 abi_type_align(abi_t* abi, abi_type_t* type);

static u32 abi_field_size(abi_t* abi, abi_field_t* field) {
  if (field->cap) {
    return 4 * field->cap;
  }
  if (field->val.kind == ABI_VAL_STRUCT) {
    return abi_type_size(abi, abi_type(abi, field->val.name));
  }
  return 4;
}

static u32 abi_field_align(abi_t* abi, abi_field_t* field) {
  if (field->val.kind == ABI_VAL_STRUCT) {
    return abi_type_align(abi, abi_type(abi, field->val.name));
  }
  return 4;
}

static u32 abi_align_up(u32 value, u32 align) {
  return (value + align - 1) & ~(align - 1);
}

static u32 abi_type_align(abi_t* abi, abi_type_t* type) {
  u32 align = 1;
  sp_da_for(type->fields, it) {
    u32 field_align = abi_field_align(abi, &type->fields[it]);
    if (field_align > align) {
      align = field_align;
    }
  }
  return align;
}

static u32 abi_type_size(abi_t* abi, abi_type_t* type) {
  u32 size = 0;
  sp_da_for(type->fields, it) {
    abi_field_t* field = &type->fields[it];
    size = abi_align_up(size, abi_field_align(abi, field));
    size += abi_field_size(abi, field);
  }
  return abi_align_up(size, abi_type_align(abi, type));
}

static sp_str_t abi_export_sig(abi_t* abi, abi_export_t* export) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(abi->mem, &w);

  sp_fmt_io(&w.base, "(");
  sp_da_for(export->args, it) {
    sp_fmt_io(&w.base, "{}", sp_fmt_cstr(export->args[it].kind == ABI_VAL_STR ? "$" : "i"));
  }
  sp_fmt_io(&w.base, ")");
  if (export->ret.kind != ABI_VAL_VOID) {
    sp_fmt_io(&w.base, "i");
  }
  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t abi_export_thunk(abi_t* abi, abi_export_t* export) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(abi->mem, &w);
  sp_io_writer_t* io = &w.base;

  sp_str_t host = sp_str_empty(export->host) ? export->name : export->host;
  sp_str_t name = sp_str_strip_left(export->name, sp_str_lit("spn_"));

  const c8* ret_type = "u32";
  const c8* fail = "return 0;";
  switch (export->ret.kind) {
    case ABI_VAL_VOID: ret_type = "void"; fail = "return;"; break;
    case ABI_VAL_S32:  ret_type = "s32"; fail = "return SPN_ERROR;"; break;
    case ABI_VAL_STR:
    case ABI_VAL_STR_OPT:
    case ABI_VAL_HANDLE:
    case ABI_VAL_STRUCT: break;
  }

  sp_fmt_io(io, "static {} spn_abi_thunk_{}(wasm_exec_env_t env", sp_fmt_cstr(ret_type), sp_fmt_str(name));
  sp_da_for(export->args, it) {
    const c8* type = "u32";
    switch (export->args[it].kind) {
      case ABI_VAL_STR: type = "const c8*"; break;
      case ABI_VAL_S32: type = "s32"; break;
      case ABI_VAL_VOID:
      case ABI_VAL_STR_OPT:
      case ABI_VAL_HANDLE:
      case ABI_VAL_STRUCT: break;
    }
    sp_fmt_io(io, ", {} a{}", sp_fmt_cstr(type), sp_fmt_uint(it));
  }
  sp_fmt_io(io, ") {{\n");
  sp_fmt_io(io, "  spn_wasm_ctx_t abi = spn_wasm_ctx(env);\n");

  sp_da_for(export->args, it) {
    if (export->args[it].kind == ABI_VAL_STR_OPT) {
      sp_fmt_io(io, "  const c8* s{} = SP_NULLPTR;\n", sp_fmt_uint(it));
      sp_fmt_io(io, "  if (!spn_wasm_read_str(&abi, a{}, &s{})) {}\n", sp_fmt_uint(it), sp_fmt_uint(it), sp_fmt_cstr(fail));
      continue;
    }
    if (export->args[it].kind != ABI_VAL_HANDLE) {
      continue;
    }
    sp_fmt_io(io, "  void* h{} = spn_wasm_resolve_handle(abi.handles, a{}, {});\n", sp_fmt_uint(it), sp_fmt_uint(it), sp_fmt_str(abi_val_kind(abi, export->args[it])));
    sp_fmt_io(io, "  if (!h{}) {}\n", sp_fmt_uint(it), sp_fmt_cstr(fail));
  }

  sp_io_dyn_mem_writer_t args = sp_zero;
  sp_io_dyn_mem_writer_init(abi->mem, &args);
  if (export->wasm_ctx) {
    sp_fmt_io(&args.base, "&abi");
  }
  sp_da_for(export->args, it) {
    const c8* prefix = "a";
    if (export->args[it].kind == ABI_VAL_HANDLE) {
      prefix = "h";
    }
    if (export->args[it].kind == ABI_VAL_STR_OPT) {
      prefix = "s";
    }
    sp_fmt_io(&args.base, "{}{}{}",
      sp_fmt_cstr((it || export->wasm_ctx) ? ", " : ""),
      sp_fmt_cstr(prefix),
      sp_fmt_uint(it));
  }
  sp_str_t call = sp_io_dyn_mem_writer_take_str(&args);

  switch (export->ret.kind) {
    case ABI_VAL_VOID: {
      sp_fmt_io(io, "  {}({});\n", sp_fmt_str(host), sp_fmt_str(call));
      break;
    }
    case ABI_VAL_S32: {
      sp_fmt_io(io, "  return {}({});\n", sp_fmt_str(host), sp_fmt_str(call));
      break;
    }
    case ABI_VAL_STR: {
      sp_fmt_io(io, "  return spn_wasm_copy_str(&abi, {}({}));\n", sp_fmt_str(host), sp_fmt_str(call));
      break;
    }
    case ABI_VAL_STR_OPT:
    case ABI_VAL_HANDLE:
    case ABI_VAL_STRUCT: {
      sp_fmt_io(io, "  void* result = (void*)({}({}));\n", sp_fmt_str(host), sp_fmt_str(call));
      sp_fmt_io(io, "  if (!result) return 0;\n");
      sp_fmt_io(io, "  return spn_wasm_add_handle(abi.handles, result, {});\n", sp_fmt_str(abi_val_kind(abi, export->ret)));
      break;
    }
  }

  sp_fmt_io(io, "}}\n");
  return sp_io_dyn_mem_writer_take_str(&w);
}

static sp_str_t abi_field_decl(abi_t* abi, abi_field_t* field, bool wire) {
  sp_str_t type = sp_zero;
  switch (field->val.kind) {
    case ABI_VAL_STR:    type = wire ? sp_str_lit("u32") : sp_str_lit("const c8*"); break;
    case ABI_VAL_HANDLE: type = wire ? sp_str_lit("u32") : sp_fmt(abi->mem, "spn_{}_t*", sp_fmt_str(field->val.name)).value; break;
    case ABI_VAL_STRUCT: type = wire ? abi_wire_type(abi, field->val.name) : abi_struct_type(abi, field->val.name); break;
    case ABI_VAL_VOID:
    case ABI_VAL_S32:
    case ABI_VAL_STR_OPT: break;
  }

  if (field->cap) {
    return sp_fmt(abi->mem, "{} {} [{}]", sp_fmt_str(type), sp_fmt_str(field->name), sp_fmt_uint(field->cap)).value;
  }
  return sp_fmt(abi->mem, "{} {}", sp_fmt_str(type), sp_fmt_str(field->name)).value;
}

static void abi_bind_field(abi_t* abi, sp_template_scope_t* scope, abi_field_t* field) {
  sp_template_set(scope, sp_str_lit("key"), field->name);
  sp_template_set(scope, sp_str_lit("decl"), abi_field_decl(abi, field, false));
  sp_template_set(scope, sp_str_lit("wire_decl"), abi_field_decl(abi, field, true));

  if (field->cap) {
    sp_template_set(scope, sp_str_lit("cap"), sp_fmt(abi->mem, "{}", sp_fmt_uint(field->cap)).value);
    sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/str_array"));
    return;
  }
  switch (field->val.kind) {
    case ABI_VAL_STR: {
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/str"));
      break;
    }
    case ABI_VAL_HANDLE: {
      sp_template_set(scope, sp_str_lit("kind"), abi_val_kind(abi, field->val));
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/handle"));
      break;
    }
    case ABI_VAL_STRUCT: {
      sp_template_set(scope, sp_str_lit("object"), field->val.name);
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/object"));
      break;
    }
    case ABI_VAL_VOID:
    case ABI_VAL_S32:
    case ABI_VAL_STR_OPT: break;
  }
}

static sp_template_scope_t* abi_scope(abi_t* abi) {
  sp_template_scope_t* root = sp_template_scope_create(abi->mem);

  sp_om_for(abi->types, it) {
    abi_type_t* type = sp_str_om_at(abi->types, it);
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("types"));
    sp_template_set(scope, sp_str_lit("name"), type->name);
    sp_template_set(scope, sp_str_lit("type"), abi_struct_type(abi, type->name));
    sp_template_set(scope, sp_str_lit("wire_type"), abi_wire_type(abi, type->name));
    sp_template_set(scope, sp_str_lit("size"), sp_fmt(abi->mem, "{}", sp_fmt_uint(abi_type_size(abi, type))).value);
    sp_da_for(type->fields, field) {
      abi_bind_field(abi, sp_template_push(scope, sp_str_lit("fields")), &type->fields[field]);
    }
  }

  sp_om_for(abi->types, it) {
    abi_type_t* type = sp_str_om_at(abi->types, it);
    if (!type->fn) {
      continue;
    }
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("functions"));
    sp_template_set(scope, sp_str_lit("name"), type->name);
    sp_template_set(scope, sp_str_lit("type"), abi_struct_type(abi, type->name));
  }

  sp_da_for(abi->exports, it) {
    abi_export_t* export = &abi->exports[it];
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("fns"));
    sp_template_set(scope, sp_str_lit("name"), export->name);
    sp_template_set(scope, sp_str_lit("short"), sp_str_strip_left(export->name, sp_str_lit("spn_")));
    sp_template_set(scope, sp_str_lit("sig"), abi_export_sig(abi, export));
    sp_template_set(scope, sp_str_lit("thunk"), abi_export_thunk(abi, export));
  }

  return root;
}

bool abi_render_decls(abi_t* abi, sp_io_writer_t* io, sp_template_registry_t* reg) {
  return cg_render(abi->mem, &abi->err, io, reg, "decls", abi_scope(abi));
}

bool abi_render_impl(abi_t* abi, sp_io_writer_t* io, sp_template_registry_t* reg) {
  return cg_render(abi->mem, &abi->err, io, reg, "impl", abi_scope(abi));
}
