#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/atomic_file.h"
#include "codegen.h"

typedef struct {
  const c8* schema;
  const c8* out;
  const c8* templates;
} jtd_gen_args_desc_t;

#define try(expr) do { sp_cli_result_t err = (expr); if (err) return err; } while (0)

typedef struct {
  sp_mem_t mem;
  sp_mem_heap_t* heap;
  struct {
    sp_str_t schema;
    sp_str_t out;
    sp_str_t templates;
  } args;
  jtd_result_t jtd;
  sp_template_registry_t* registry;
  gen_t* gen;
} ctx_t;

sp_cli_result_t parse_schema(ctx_t* c) {
  if (jtd_parse_file(c->mem, c->args.schema, &c->jtd)) {
    sp_cli_log_error("failed to parse schema ({})", sp_fmt_str(jtd_diagnostic_message(c->mem, &c->jtd.diag)));
    return SP_CLI_ERR;
  }
  return SP_CLI_OK;
}

sp_cli_result_t walk_schema(ctx_t* c) {
  jtd_ref_t root = c->jtd.root->as.ref;
  walk_result_t walked = register_type(c->gen, root.name, root.target);
  if (walked.err) {
    sp_cli_log_error("{}", sp_fmt_str(walk_result_to_str(c->mem, walked)));
    return SP_CLI_ERR;
  }

  c->gen->root = find_type(c->gen, root.name);

  return SP_CLI_OK;
}

sp_cli_result_t render_template(ctx_t* c) {
  sp_str_t dir = c->args.templates;
  c->registry = sp_template_registry_create(c->mem);
  if (sp_template_load_dir(c->registry, dir)) {
    sp_cli_log_error("failed to load templates from {.cyan}", sp_fmt_str(dir));
    return SP_CLI_ERR;
  }

  // Open the output file, which we'll atomically rename to the final output
  sp_fs_atomic_t file = sp_zero;
  if (sp_fs_atomic_open(&file, c->args.out)) {
    sp_cli_log_error("failed to open {.cyan}", sp_fmt_str(c->args.out));
    return SP_CLI_ERR;
  }

  u8 buffer [64 * 1024];
  sp_io_writer_t* io = sp_fs_atomic_writer(&file);
  sp_io_writer_set_buffer(io, buffer, sizeof(buffer));

  render_result_t rendered = render_file(c->gen, io, c->registry);
  if (rendered.err) {
    sp_fs_atomic_abort(&file);
    sp_cli_log_error("{}", sp_fmt_str(render_result_to_str(c->mem, rendered)));
    return SP_CLI_ERR;
  }

  if (sp_fs_atomic_commit(&file, SP_FS_ATOMIC_REPLACE)) {
    sp_cli_log_error("failed to write {.cyan}", sp_fmt_str(c->args.out));
    return SP_CLI_ERR;
  }

  return SP_CLI_OK;
}

gen_t* gen_new(sp_mem_t mem) {
  gen_t* gen = sp_alloc_type(mem, gen_t);
  gen->mem = mem;
  gen->entries = sp_da_new(mem, entry_t);
  sp_str_om_init(gen->types);
  sp_str_om_init(gen->array_types);
  sp_str_om_init(gen->nodes);
  sp_str_ht_init(mem, gen->visited);
  return gen;
}

static sp_cli_result_t run_cli(sp_cli_t* cli) {
  jtd_gen_args_desc_t* parsed = sp_cast(jtd_gen_args_desc_t*, cli->user_data);

  sp_mem_heap_t* heap = sp_mem_heap_new();
  sp_mem_t mem = sp_mem_heap_as_allocator(heap);
  ctx_t ctx = {
    .heap = heap,
    .mem = mem,
    .args = {
      .schema = sp_cstr_as_str(parsed->schema),
      .out = sp_cstr_as_str(parsed->out),
      .templates = sp_cstr_as_str(parsed->templates),
    },
    .gen = gen_new(mem)
  };

  try(parse_schema(&ctx));
  try(walk_schema(&ctx));
  try(render_template(&ctx));

  sp_log("wrote {} types to {.cyan}", sp_fmt_uint(sp_str_om_size(ctx.gen->types)), sp_fmt_str(ctx.args.out));
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  jtd_gen_args_desc_t parsed = sp_zero;

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
        .summary = "Path to the generated header (.gen.h)",
        .ptr = &parsed.out,
      },
      {
        .name = "templates",
        .summary = "Path to the template directory",
        .ptr = &parsed.templates,
      },
    },
    .handler = run_cli,
  };

  return sp_cli_main(&root, num_args, args, &parsed);
}
