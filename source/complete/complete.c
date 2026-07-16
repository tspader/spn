#include "complete/complete.h"

#include "profile/types.h"
#include "toolchain/types.h"

#define SPN_COMPLETE_MAX_POSITIONALS 16

typedef struct {
  spn_complete_desc_t desc;
  sp_str_t current;
  sp_cli_cmd_t* path [SP_CLI_MAX_DEPTH];
  u32 depth;
  bool raw;
  sp_cli_opt_t* pending;
  sp_str_t positionals [SPN_COMPLETE_MAX_POSITIONALS];
  u32 num_positionals;
  struct {
    bool bin;
    bool lib;
    bool test;
    bool script;
  } only;
} spn_complete_ctx_t;

static void emit(spn_complete_ctx_t* ctx, sp_str_t candidate) {
  if (!sp_str_starts_with(candidate, ctx->current)) {
    return;
  }
  sp_for(it, ctx->num_positionals) {
    if (sp_str_equal(ctx->positionals[it], candidate)) {
      return;
    }
  }
  sp_fmt_io(ctx->desc.io, "{}\n", sp_fmt_str(candidate));
}

static void emit_cstr(spn_complete_ctx_t* ctx, const c8* candidate) {
  emit(ctx, sp_cstr_as_str(candidate));
}

static void emit_flag(spn_complete_ctx_t* ctx, const c8* name) {
  c8 buffer [SP_CLI_MAX_LABEL];
  sp_io_mem_writer_t label = sp_zero;
  sp_io_mem_writer_from_buffer(&label, buffer, SP_CLI_MAX_LABEL);
  sp_fmt_io(&label.base, "--{}", sp_fmt_cstr(name));
  emit(ctx, sp_io_mem_writer_as_str(&label));
}

static bool has_commands(sp_cli_cmd_t* cmd) {
  return cmd->commands[0] != SP_NULLPTR;
}

static sp_cli_cmd_t* find_command(sp_cli_cmd_t* cmd, sp_str_t name) {
  sp_carr_for_until(cmd->commands, it, cmd->commands[it]) {
    if (sp_str_equal_cstr(name, cmd->commands[it]->name)) {
      return cmd->commands[it];
    }
  }
  return SP_NULLPTR;
}

static sp_cli_opt_t* find_opt_in(sp_cli_cmd_t* cmd, sp_str_t name) {
  sp_carr_for_until(cmd->opts, it, cmd->opts[it].name) {
    if (sp_str_equal_cstr(name, cmd->opts[it].name)) {
      return &cmd->opts[it];
    }
  }
  return SP_NULLPTR;
}

static sp_cli_opt_t* find_opt(spn_complete_ctx_t* ctx, sp_str_t name) {
  sp_for(it, ctx->depth) {
    sp_cli_opt_t* opt = find_opt_in(ctx->path[ctx->depth - 1 - it], name);
    if (opt) {
      return opt;
    }
  }
  return SP_NULLPTR;
}

static sp_cli_opt_t* find_brief_in(sp_cli_cmd_t* cmd, c8 brief) {
  sp_carr_for_until(cmd->opts, it, cmd->opts[it].name) {
    if (cmd->opts[it].brief && cmd->opts[it].brief[0] == brief) {
      return &cmd->opts[it];
    }
  }
  return SP_NULLPTR;
}

static sp_cli_opt_t* find_brief(spn_complete_ctx_t* ctx, c8 brief) {
  sp_for(it, ctx->depth) {
    sp_cli_opt_t* opt = find_brief_in(ctx->path[ctx->depth - 1 - it], brief);
    if (opt) {
      return opt;
    }
  }
  return SP_NULLPTR;
}

static void mark_filter(spn_complete_ctx_t* ctx, sp_cli_opt_t* opt) {
  if (opt->kind != SP_CLI_OPT_BOOLEAN) {
    return;
  }
  if (sp_cstr_equal(opt->name, "bin")) {
    ctx->only.bin = true;
  }
  else if (sp_cstr_equal(opt->name, "lib")) {
    ctx->only.lib = true;
  }
  else if (sp_cstr_equal(opt->name, "test")) {
    ctx->only.test = true;
  }
  else if (sp_cstr_equal(opt->name, "script")) {
    ctx->only.script = true;
  }
}

static void scan_long(spn_complete_ctx_t* ctx, sp_str_t word) {
  sp_str_t body = sp_str_strip_left(word, sp_str_lit("--"));

  sp_str_t name = body;
  bool has_value = false;
  sp_str_for(body, it) {
    if (sp_str_at(body, it) == '=') {
      name = sp_str_prefix(body, it);
      has_value = true;
      break;
    }
  }

  sp_cli_opt_t* opt = find_opt(ctx, name);
  if (!opt) {
    return;
  }

  mark_filter(ctx, opt);
  if (!has_value && opt->kind != SP_CLI_OPT_BOOLEAN) {
    ctx->pending = opt;
  }
}

static void scan_briefs(spn_complete_ctx_t* ctx, sp_str_t word) {
  sp_str_t cluster = sp_str_strip_left(word, sp_str_lit("-"));
  sp_str_for(cluster, it) {
    sp_cli_opt_t* opt = find_brief(ctx, sp_str_at(cluster, it));
    if (!opt) {
      return;
    }

    mark_filter(ctx, opt);
    if (opt->kind != SP_CLI_OPT_BOOLEAN) {
      if (it + 1 >= cluster.len) {
        ctx->pending = opt;
      }
      return;
    }
  }
}

static void put_opt(sp_cli_opt_t** opts, u32* num_opts, sp_cli_opt_t* opt) {
  sp_for(it, *num_opts) {
    if (sp_cstr_equal(opts[it]->name, opt->name)) {
      opts[it] = opt;
      return;
    }
  }
  opts[(*num_opts)++] = opt;
}

static void put_opts(sp_cli_cmd_t* cmd, sp_cli_opt_t** opts, u32* num_opts) {
  sp_carr_for_until(cmd->opts, it, cmd->opts[it].name) {
    put_opt(opts, num_opts, &cmd->opts[it]);
  }
}

static void complete_opts(spn_complete_ctx_t* ctx) {
  sp_cli_opt_t* opts [SP_CLI_MAX_OPTS * SP_CLI_MAX_DEPTH] = sp_zero;
  u32 num_opts = 0;

  sp_for(it, ctx->depth) {
    put_opts(ctx->path[it], opts, &num_opts);
  }

  sp_for(it, num_opts) {
    emit_flag(ctx, opts[it]->name);
  }
  emit_flag(ctx, "help");
}

static void complete_commands(spn_complete_ctx_t* ctx, sp_cli_cmd_t* cmd) {
  sp_carr_for_until(cmd->commands, it, cmd->commands[it]) {
    emit_cstr(ctx, cmd->commands[it]->name);
  }
}

static void complete_targets(spn_complete_ctx_t* ctx, spn_target_map_t targets) {
  sp_str_om_for(targets, it) {
    emit(ctx, sp_str_om_at(targets, it)->name);
  }
}

static bool is_builtin_profile(sp_str_t name) {
  return sp_str_equal_cstr(name, "default") ||
         sp_str_equal_cstr(name, "debug") ||
         sp_str_equal_cstr(name, "release");
}

static void complete_value(spn_complete_ctx_t* ctx, sp_cli_opt_t* opt) {
  spn_pkg_info_t* pkg = ctx->desc.pkg;

  if (sp_cstr_equal(opt->name, "profile")) {
    emit_cstr(ctx, "default");
    emit_cstr(ctx, "debug");
    emit_cstr(ctx, "release");
    if (!pkg) {
      return;
    }
    sp_str_om_for(pkg->profiles, it) {
      sp_str_t name = sp_str_om_at(pkg->profiles, it)->name;
      if (!is_builtin_profile(name)) {
        emit(ctx, name);
      }
    }
  }
  else if (sp_cstr_equal(opt->name, "mode")) {
    emit_cstr(ctx, "debug");
    emit_cstr(ctx, "release");
  }
  else if (sp_cstr_equal(opt->name, "opt")) {
    const c8* levels [] = { "0", "1", "2", "3", "s", "z" };
    sp_carr_for(levels, it) {
      emit_cstr(ctx, levels[it]);
    }
  }
  else if (sp_cstr_equal(opt->name, "toolchain")) {
    emit_cstr(ctx, "zig");
    if (!pkg) {
      return;
    }
    sp_str_om_for(pkg->toolchains, it) {
      sp_str_t name = sp_str_om_at(pkg->toolchains, it)->name;
      if (!sp_str_equal_cstr(name, "zig")) {
        emit(ctx, name);
      }
    }
  }
}

static sp_cli_arg_t* positional_arg(sp_cli_cmd_t* cmd, u32 index) {
  u32 num_fixed = 0;
  sp_carr_for_until(cmd->args, it, cmd->args[it].name) {
    if (cmd->args[it].arity == SP_CLI_ARG_REST) {
      break;
    }
    num_fixed++;
  }

  if (index < num_fixed) {
    return &cmd->args[index];
  }
  sp_carr_for_until(cmd->args, it, cmd->args[it].name) {
    if (cmd->args[it].arity == SP_CLI_ARG_REST) {
      return &cmd->args[it];
    }
  }
  return SP_NULLPTR;
}

static void complete_positional(spn_complete_ctx_t* ctx, sp_cli_cmd_t* cmd) {
  sp_cli_arg_t* arg = positional_arg(cmd, ctx->num_positionals);
  if (!arg) {
    return;
  }

  if (sp_cstr_equal(cmd->name, "completions") && sp_cstr_equal(arg->name, "shell")) {
    const c8* shells [] = { "bash", "zsh", "fish", "powershell" };
    sp_carr_for(shells, it) {
      emit_cstr(ctx, shells[it]);
    }
    return;
  }

  spn_pkg_info_t* pkg = ctx->desc.pkg;
  if (!pkg) {
    return;
  }

  if (sp_cstr_equal(cmd->name, "build") && sp_cstr_equal(arg->name, "name")) {
    bool any = ctx->only.bin || ctx->only.lib || ctx->only.test || ctx->only.script;
    if (!any || ctx->only.bin) {
      complete_targets(ctx, pkg->exes);
    }
    if (!any || ctx->only.lib) {
      complete_targets(ctx, pkg->libs);
    }
    if (!any || ctx->only.test) {
      complete_targets(ctx, pkg->tests);
    }
    if (!any || ctx->only.script) {
      complete_targets(ctx, pkg->scripts);
    }
  }
  else if (sp_cstr_equal(cmd->name, "test") && sp_cstr_equal(arg->name, "name")) {
    complete_targets(ctx, pkg->tests);
  }
  else if (sp_cstr_equal(cmd->name, "run") && sp_cstr_equal(arg->name, "entry")) {
    complete_targets(ctx, pkg->scripts);
  }
}

void spn_complete(spn_complete_desc_t desc) {
  if (!desc.root || !desc.io || desc.num_words < 2) {
    return;
  }

  spn_complete_ctx_t ctx = sp_zero;
  ctx.desc = desc;
  ctx.path[ctx.depth++] = desc.root;
  ctx.current = sp_cstr_as_str(desc.words[desc.num_words - 1]);

  for (u32 it = 1; it < desc.num_words - 1; it++) {
    sp_str_t word = sp_cstr_as_str(desc.words[it]);
    sp_cli_cmd_t* leaf = ctx.path[ctx.depth - 1];

    if (ctx.pending) {
      ctx.pending = SP_NULLPTR;
    }
    else if (!ctx.raw && sp_str_equal(word, sp_str_lit("--"))) {
      ctx.raw = true;
    }
    else if (!ctx.raw && sp_str_starts_with(word, sp_str_lit("--"))) {
      scan_long(&ctx, word);
    }
    else if (!ctx.raw && word.len > 1 && sp_str_at(word, 0) == '-') {
      scan_briefs(&ctx, word);
    }
    else if (has_commands(leaf)) {
      sp_cli_cmd_t* sub = find_command(leaf, word);
      if (sub && ctx.depth < SP_CLI_MAX_DEPTH) {
        ctx.path[ctx.depth++] = sub;
      }
    }
    else if (ctx.num_positionals < SPN_COMPLETE_MAX_POSITIONALS) {
      ctx.positionals[ctx.num_positionals++] = word;
    }
  }

  if (ctx.pending) {
    complete_value(&ctx, ctx.pending);
    return;
  }

  if (!ctx.raw && ctx.current.len && sp_str_at(ctx.current, 0) == '-') {
    complete_opts(&ctx);
    return;
  }

  sp_cli_cmd_t* leaf = ctx.path[ctx.depth - 1];
  if (has_commands(leaf)) {
    complete_commands(&ctx, leaf);
    return;
  }

  complete_positional(&ctx, leaf);
}
