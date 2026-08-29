#define SP_TEMPLATE_IMPLEMENTATION
#include "render.h"
#include "sp_template.h"

#define try(expr) do { if (!(expr)) return false; } while (0)

typedef struct {
  sp_str_t name;
  sp_str_t asset;
  sp_str_t sha;
  sp_str_t exe;
  sp_str_t kind;
  bool windows;
} target_t;

typedef struct {
  sp_mem_t mem;
  installer_config_t config;
  sp_da(target_t) targets;
  sp_template_registry_t* reg;
  installer_result_t result;
} installer_t;

static bool fail(installer_t* in, installer_err_t err, u32 line, sp_str_t subject) {
  in->result = (installer_result_t) { .err = err, .line = line, .subject = subject };
  return false;
}

static bool sha_valid(sp_str_t sha) {
  if (sha.len != 64) {
    return false;
  }
  sp_str_for(sha, it) {
    c8 c = sha.data[it];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

static bool classify(installer_t* in, u32 line, sp_str_t asset, target_t* target) {
  sp_str_t prefix = sp_str_lit("spn-");
  sp_str_t tar = sp_str_lit(".tar.gz");
  sp_str_t zip = sp_str_lit(".zip");

  sp_str_t ext = sp_zero;
  if (sp_str_ends_with(asset, tar)) {
    ext = tar;
    target->kind = sp_str_lit("tar");
  }
  else if (sp_str_ends_with(asset, zip)) {
    ext = zip;
    target->kind = sp_str_lit("zip");
  }

  bool named = sp_str_starts_with(asset, prefix) && ext.len && asset.len > prefix.len + ext.len;
  if (!named) {
    return fail(in, INSTALLER_ERR_ASSET, line, asset);
  }

  target->asset = asset;
  target->name = sp_str_sub(asset, (s32)prefix.len, (s32)(asset.len - prefix.len - ext.len));
  target->windows = sp_str_ends_with(target->name, sp_str_lit("-windows"));
  target->exe = target->windows ? sp_str_lit("spn.exe") : sp_str_lit("spn");

  if (target->windows != sp_str_equal_cstr(target->kind, "zip")) {
    return fail(in, INSTALLER_ERR_PAIRING, line, asset);
  }
  return true;
}

static bool parse_line(installer_t* in, u32 line, sp_str_t text) {
  sp_str_pair_t pair = sp_str_cleave_c8(text, ' ');
  sp_str_t sha = pair.first;
  sp_str_t asset = sp_str_trim(pair.second);
  if (sp_str_starts_with(asset, sp_str_lit("*"))) {
    asset = sp_str_sub(asset, 1, (s32)asset.len - 1);
  }

  if (!asset.len) {
    return fail(in, INSTALLER_ERR_FIELDS, line, text);
  }
  if (!sha_valid(sha)) {
    return fail(in, INSTALLER_ERR_SHA, line, sha);
  }

  target_t target = sp_zero;
  target.sha = sha;
  try(classify(in, line, asset, &target));

  sp_da_for(in->targets, it) {
    if (sp_str_equal(in->targets[it].name, target.name)) {
      return fail(in, INSTALLER_ERR_DUPLICATE, line, target.name);
    }
  }
  sp_da_push(in->targets, target);
  return true;
}

static s32 sort_targets(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const target_t*)a)->name, ((const target_t*)b)->name);
}

static bool parse(installer_t* in) {
  sp_da(sp_str_t) lines = sp_str_split_c8(in->mem, in->config.shasums, '\n');
  sp_da_for(lines, it) {
    sp_str_t line = sp_str_trim(lines[it]);
    if (!line.len) {
      continue;
    }
    try(parse_line(in, it + 1, line));
  }

  if (sp_da_empty(in->targets)) {
    return fail(in, INSTALLER_ERR_EMPTY, 0, sp_zero_s(sp_str_t));
  }
  sp_da_sort(in->targets, sort_targets);
  return true;
}

static void bind_target(sp_template_scope_t* scope, target_t* target) {
  sp_template_set(scope, sp_str_lit("name"), target->name);
  sp_template_set(scope, sp_str_lit("asset"), target->asset);
  sp_template_set(scope, sp_str_lit("sha"), target->sha);
  sp_template_set(scope, sp_str_lit("exe"), target->exe);
  sp_template_set(scope, sp_str_lit("kind"), target->kind);
}

static sp_template_scope_t* bind_root(installer_t* in) {
  sp_template_scope_t* root = sp_template_scope_create(in->mem);
  sp_template_set(root, sp_str_lit("version"), in->config.version);
  sp_template_set(root, sp_str_lit("tag"), in->config.tag);
  sp_template_set(root, sp_str_lit("repo"), in->config.repo);
  sp_template_list(root, sp_str_lit("targets"));
  sp_template_list(root, sp_str_lit("windows_targets"));

  sp_da_for(in->targets, it) {
    target_t* target = &in->targets[it];
    bind_target(sp_template_push(root, sp_str_lit("targets")), target);
    if (target->windows) {
      bind_target(sp_template_push(root, sp_str_lit("windows_targets")), target);
    }
  }
  return root;
}

static bool render_one(installer_t* in, sp_str_t name, sp_template_scope_t* scope) {
  sp_str_t source = sp_zero;
  if (!sp_template_get(in->reg, name, &source)) {
    return fail(in, INSTALLER_ERR_TEMPLATES, 0, name);
  }

  sp_io_dyn_mem_writer_t body = sp_zero;
  sp_io_dyn_mem_writer_init(in->mem, &body);
  if (sp_template_render(&body.base, source, scope, in->reg)) {
    return fail(in, INSTALLER_ERR_RENDER, 0, name);
  }

  sp_str_t path = sp_fs_join_path(in->mem, in->config.out, name);
  sp_str_t content = sp_io_dyn_mem_writer_as_str(&body);
  sp_io_file_writer_t file = sp_zero;
  if (sp_io_file_writer_from_path(&file, path)) {
    return fail(in, INSTALLER_ERR_IO, 0, path);
  }
  sp_err_t written = sp_io_write_all(&file.base, content.data, content.len, SP_NULLPTR);
  sp_err_t closed = sp_io_file_writer_close(&file);
  if (written || closed) {
    return fail(in, INSTALLER_ERR_IO, 0, path);
  }
  return true;
}

static bool run(installer_t* in) {
  in->reg = sp_template_registry_create(in->mem);
  if (!sp_fs_is_dir(in->config.templates) || sp_template_load_dir(in->reg, in->config.templates)) {
    return fail(in, INSTALLER_ERR_TEMPLATES, 0, in->config.templates);
  }

  try(parse(in));

  sp_template_scope_t* scope = bind_root(in);
  try(render_one(in, sp_str_lit("install.sh"), scope));
  try(render_one(in, sp_str_lit("install.ps1"), scope));
  return true;
}

installer_result_t installer_render(sp_mem_t mem, installer_config_t config) {
  installer_t in = {
    .mem = mem,
    .config = config,
    .targets = sp_da_new(mem, target_t),
  };
  run(&in);
  return in.result;
}

sp_str_t installer_result_to_str(sp_mem_t mem, installer_result_t result) {
  switch (result.err) {
    case INSTALLER_OK:               return sp_str_lit("ok");
    case INSTALLER_ERR_IO:           return sp_fmt(mem, "failed to write {}", sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_TEMPLATES:    return sp_fmt(mem, "failed to load template {}", sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_RENDER:       return sp_fmt(mem, "failed to render template {}", sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_FIELDS:       return sp_fmt(mem, "line {}: expected a sha and an asset, got {}", sp_fmt_uint(result.line), sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_SHA:          return sp_fmt(mem, "line {}: {} is not a lowercase sha256", sp_fmt_uint(result.line), sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_ASSET:        return sp_fmt(mem, "line {}: {} is not a spn release asset", sp_fmt_uint(result.line), sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_PAIRING:      return sp_fmt(mem, "line {}: {} pairs a platform with the wrong archive format", sp_fmt_uint(result.line), sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_DUPLICATE:    return sp_fmt(mem, "line {}: duplicate target {}", sp_fmt_uint(result.line), sp_fmt_str(result.subject)).value;
    case INSTALLER_ERR_EMPTY:        return sp_str_lit("no assets");
  }
  return sp_str_lit("unknown error");
}
