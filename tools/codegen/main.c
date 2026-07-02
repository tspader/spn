#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/atomic_file.h"
#include "codegen.h"

#define try(expr) do { sp_cli_result_t err = (expr); if (err) return err; } while (0)

typedef struct {
  const c8* schema;
  const c8* out;
  const c8* templates;
} args_t;

typedef struct {
  sp_str_t name;
  sp_str_t renderer;
  jtd_schema_t* schema;
} root_t;

typedef struct {
  sp_cli_t* cli;
  sp_mem_t mem;
  struct {
    sp_str_t schema;
    sp_str_t out;
    sp_str_t templates;
  } args;
  jtd_result_t jtd;
  sp_da(root_t) roots;
} ctx_t;

typedef bool (*render_fn_t)(gen_t*, sp_io_writer_t*, sp_template_registry_t*);

static sp_cli_result_t parse_schema(ctx_t* c) {
  if (jtd_parse_file(c->mem, c->args.schema, &c->jtd)) {
    return sp_cli_set_error(c->cli, sp_fmt(c->mem, "failed to parse schema ({})", sp_fmt_str(jtd_diagnostic_message(c->mem, &c->jtd.diag))).value);
  }
  return SP_CLI_OK;
}

static sp_cli_result_t collect_roots(ctx_t* c) {
  c->roots = sp_da_new(c->mem, root_t);
  sp_da_for(c->jtd.definitions, it) {
    jtd_definition_t* def = &c->jtd.definitions[it];
    if (!jtd_metadata_has(def->schema, "renderer")) {
      continue;
    }
    root_t root = {
      .name = def->name,
      .renderer = jtd_metadata(def->schema, "renderer"),
      .schema = def->schema,
    };
    sp_da_push(c->roots, root);
  }

  if (sp_da_empty(c->roots)) {
    return sp_cli_set_error_c(c->cli, "schema has no roots (a definition with metadata.renderer)");
  }
  return SP_CLI_OK;
}

static sp_cli_result_t extract_root(ctx_t* c, gen_t* gen, root_t* root) {
  if (!gen_extract(gen, root->name, root->schema)) {
    return sp_cli_set_error(c->cli, gen->err);
  }
  return SP_CLI_OK;
}

static sp_cli_result_t load_registry(ctx_t* c, sp_str_t renderer, sp_template_registry_t** out) {
  sp_template_registry_t* reg = sp_template_registry_create(c->mem);
  sp_str_t dirs [] = { renderer, sp_str_lit("shared") };
  sp_carr_for(dirs, it) {
    sp_str_t dir = sp_fs_join_path(c->mem, c->args.templates, dirs[it]);
    if (!sp_fs_is_dir(dir) || sp_template_load_dir(reg, dir)) {
      return sp_cli_set_error(c->cli, sp_fmt(c->mem, "failed to load templates from {.cyan}", sp_fmt_str(dir)).value);
    }
  }
  *out = reg;
  return SP_CLI_OK;
}

static sp_cli_result_t render_one(ctx_t* c, sp_str_t path, gen_t* gen, sp_template_registry_t* reg, render_fn_t fn) {
  sp_fs_atomic_t file = sp_zero;
  if (sp_fs_atomic_open(&file, path)) {
    return sp_cli_set_error(c->cli, sp_fmt(c->mem, "failed to open {.cyan}", sp_fmt_str(path)).value);
  }

  u8 buffer [64 * 1024];
  sp_io_writer_t* io = sp_fs_atomic_writer(&file);
  sp_io_writer_set_buffer(io, buffer, sizeof(buffer));

  if (!fn(gen, io, reg)) {
    sp_fs_atomic_abort(&file);
    return sp_cli_set_error(c->cli, gen->err);
  }

  if (sp_fs_atomic_commit(&file, SP_FS_ATOMIC_REPLACE)) {
    return sp_cli_set_error(c->cli, sp_fmt(c->mem, "failed to write {.cyan}", sp_fmt_str(path)).value);
  }

  return SP_CLI_OK;
}

static sp_str_t out_path(ctx_t* c, sp_str_t name, const c8* suffix) {
  sp_str_t file = sp_fmt(c->mem, "{}{}", sp_fmt_str(name), sp_fmt_cstr(suffix)).value;
  return sp_fs_join_path(c->mem, c->args.out, file);
}

static sp_cli_result_t render_types_header(ctx_t* c) {
  sp_template_registry_t* reg = SP_NULLPTR;
  try(load_registry(c, sp_str_lit("types"), &reg));

  gen_t* gen = gen_new(c->mem);
  sp_da_for(c->roots, it) {
    try(extract_root(c, gen, &c->roots[it]));
  }

  sp_str_t path = out_path(c, sp_str_lit("types"), ".gen.h");
  try(render_one(c, path, gen, reg, render_types));
  sp_log("wrote {} types to {.cyan}", sp_fmt_uint(sp_str_om_size(gen->types)), sp_fmt_str(path));
  return SP_CLI_OK;
}

static sp_cli_result_t render_root(ctx_t* c, root_t* root) {
  sp_template_registry_t* reg = SP_NULLPTR;
  try(load_registry(c, root->renderer, &reg));

  gen_t* gen = gen_new(c->mem);
  try(extract_root(c, gen, root));
  gen->root = gen_type(gen, root->name);

  try(render_one(c, out_path(c, root->name, ".gen.h"), gen, reg, render_decls));
  try(render_one(c, out_path(c, root->name, ".gen.c"), gen, reg, render_impl));
  sp_log("wrote {.cyan} ({.cyan})", sp_fmt_str(root->name), sp_fmt_str(root->renderer));
  return SP_CLI_OK;
}

static sp_cli_result_t run_cli(sp_cli_t* cli) {
  args_t* args = sp_cast(args_t*, cli->user_data);

  ctx_t ctx = {
    .cli = cli,
    .mem = sp_mem_heap_as_allocator(sp_mem_heap_new()),
    .args = {
      .schema = sp_cstr_as_str(args->schema),
      .out = sp_cstr_as_str(args->out),
      .templates = sp_cstr_as_str(args->templates),
    },
  };

  try(parse_schema(&ctx));
  try(collect_roots(&ctx));
  try(render_types_header(&ctx));
  sp_da_for(ctx.roots, it) {
    try(render_root(&ctx, &ctx.roots[it]));
  }

  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  args_t parsed = sp_zero;

  sp_cli_cmd_t root = {
    .name = "jtd_gen",
    .summary = "Generate C structs and load/write code from a JTD schema",
    .args = {
      {
        .name = "schema",
        .summary = "Path to the JTD schema (.jtd.json)",
        .ptr = &parsed.schema,
      },
      {
        .name = "out",
        .summary = "Path to the generated output directory",
        .ptr = &parsed.out,
      },
      {
        .name = "templates",
        .summary = "Path to the template root directory",
        .ptr = &parsed.templates,
      },
    },
    .handler = run_cli,
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = args,
    .user_data = &parsed,
  });
}
