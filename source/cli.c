#include "app.h"
#include "pkg.h"
#include "sp/io.h"


///////////
// ENUMS //
///////////
static sp_str_t spn_cli_opt_kind_to_str(spn_cli_opt_kind_t kind) {
  switch (kind) {
    SPN_CLI_OPT_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cli_arg_kind_t spn_cli_arg_kind_from_str(sp_str_t str) {
  SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_CLI_ARG_KIND_REQUIRED);
}

sp_str_t spn_cli_arg_kind_to_str(spn_cli_arg_kind_t kind) {
  switch (kind) {
    SPN_CLI_ARG_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cli_cmd_t spn_cli_command_from_str(sp_str_t str) {
  SPN_CLI_COMMAND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_CLI_LS);
}

sp_str_t spn_cli_command_to_str(spn_cli_cmd_t cmd) {
  switch (cmd) {
    SPN_CLI_COMMAND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_tool_cmd_t spn_tool_subcommand_from_str(sp_str_t str) {
  SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_TOOL_LIST);
}

sp_str_t spn_tool_subcommand_to_str(spn_tool_cmd_t cmd) {
  switch (cmd) {
    SPN_TOOL_SUBCOMMAND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static void sp_sh_ls(sp_str_t path) {
  if (!sp_fs_exists(path)) {
    SP_LOG("{:fg brightcyan} hasn't been built for your configuration", SP_FMT_STR(path));
    return;
  }

  struct {
    const c8* command;
    const c8* args [4];
  } tools [4] = {
    { "lsd", "--tree", "--depth", "2" },
    { "tree", "-L", "2" },
    { "ls" },
  };

  SP_CARR_FOR(tools, i) {
    if (sp_fs_is_on_path(sp_str_view(tools[i].command))) {
      sp_ps_config_t config = SP_ZERO_INITIALIZE();
      config.command = sp_str_view(tools[i].command);

      SP_CARR_FOR(tools[i].args, j) {
        const c8* arg = tools[i].args[j];
        if (!arg) break;
        sp_ps_config_add_arg(&config, sp_str_view(arg));
      }

      sp_ps_config_add_arg(&config, path);

      sp_ps_output_t result = sp_ps_run(config);
      SP_ASSERT(!result.status.exit_code);
      SP_LOG("{}", SP_FMT_STR(sp_str_trim(result.out)));
      return;
    }
  }
}


/////////
// CLI //
/////////
spn_cli_command_info_t spn_cli_command_info_from_usage(spn_cli_command_usage_t cmd) {
  spn_cli_command_info_t info = {
    .name = sp_str_from_cstr(cmd.name),
    .usage = sp_str_from_cstr(cmd.usage),
    .summary = sp_str_from_cstr(cmd.summary),
  };

  // Process options
  sp_carr_for(cmd.opts, it) {
    if (!cmd.opts[it].name) break;

    spn_cli_opt_info_t opt = {
      .brief = sp_str_from_cstr(cmd.opts[it].brief),
      .name = sp_str_from_cstr(cmd.opts[it].name),
      .kind = cmd.opts[it].kind,
      .summary = sp_str_from_cstr(cmd.opts[it].summary),
      .placeholder = sp_str_from_cstr(cmd.opts[it].placeholder ? cmd.opts[it].placeholder : ""),
    };
    sp_da_push(info.opts, opt);
  }

  // Process arguments
  sp_carr_for(cmd.args, it) {
    if (!cmd.args[it].name) break;

    spn_cli_arg_info_t arg = {
      .name = sp_str_from_cstr(cmd.args[it].name),
      .kind = cmd.args[it].kind,
      .summary = sp_str_from_cstr(cmd.args[it].summary),
    };
    sp_da_push(info.args, arg);
    sp_da_push(info.brief, arg.name);
  }

  return info;
}

sp_str_t spn_cli_command_usage(spn_cli_command_usage_t cmd) {
  spn_cli_command_info_t info = spn_cli_command_info_from_usage(cmd);

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  SP_ASSERT(!sp_str_empty(info.summary));
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd.summary));
  sp_str_builder_new_line(&builder);

  if (!sp_dyn_array_empty(info.opts)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.opts, it) {
      spn_cli_opt_info_t opt = info.opts[it];

      // Build short flag display
      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      // Build long flag display
      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);

    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  if (!sp_dyn_array_empty(info.args)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("arguments"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Name"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.args, it) {
      spn_cli_arg_info_t arg = info.args[it];

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(arg.name));
      sp_tui_table_str(&spn.tui.table, sp_str_lit("str"));
      sp_tui_table_str(&spn.tui.table, arg.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_t table = sp_tui_table_render(&spn.tui.table);

    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(table));
  }

  return sp_str_builder_to_str(&builder);
}

sp_str_t spn_cli_usage(spn_cli_command_usage_t* cmd) {
  spn_cli_command_info_t cmd_info = spn_cli_command_info_from_usage(*cmd);
  spn_cli_usage_info_t info = SP_ZERO_INITIALIZE();

  // Collect subcommands
  if (cmd->commands) {
    for (spn_cli_command_usage_t* sub = cmd->commands; sub->name; sub++) {
      spn_cli_command_info_t sub_info = spn_cli_command_info_from_usage(*sub);
      sp_dyn_array_push(info.commands, sub_info);
    }
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  if (cmd->summary) {
    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd->summary));
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  if (cmd->usage) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("usage"));
    sp_str_builder_indent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_CSTR(cmd->usage));
    sp_str_builder_dedent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  // Render opts (from the command itself)
  if (!sp_dyn_array_empty(cmd_info.opts)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(cmd_info.opts, it) {
      spn_cli_opt_info_t opt = cmd_info.opts[it];

      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
    sp_str_builder_new_line(&builder);
  }

  // Render subcommands
  if (!sp_dyn_array_empty(info.commands)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("commands"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Command"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Arguments"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.commands, it) {
      spn_cli_command_info_t command = info.commands[it];
      sp_str_t args = sp_str_join_n(command.brief, sp_dyn_array_size(command.brief), sp_str_lit(", "));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightcyan}", SP_FMT_STR(command.name));
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(args));
      sp_tui_table_str(&spn.tui.table, command.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  return sp_str_builder_to_str(&builder);
}

sp_app_result_t spn_cli_help(spn_cli_parser_t* p) {
  if (!p->resolved) {
    sp_log(spn_cli_command_usage(*p->cmd));
    return SP_APP_QUIT;
  }
  else if (!p->resolved->commands) {
    sp_log(spn_cli_command_usage(*p->resolved));
    return SP_APP_QUIT;
  }
  else {
    sp_log(spn_cli_command_usage(*p->resolved->commands));
    return SP_APP_QUIT;
  }
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_set_profile(spn_app_t* app, sp_str_t name) {
  if (sp_str_empty(name)) {
    app->config.profile = spn_pkg_get_default_profile(&app->package);
    return SP_APP_CONTINUE;
  }

  if (!sp_om_has(app->package.profiles, name)) {
    spn_log_error("{:fg brightcyan} profile isn't defined in {:fg brightcyan}",
      SP_FMT_STR(name),
      SP_FMT_STR(app->package.paths.manifest)
    );
    return SP_APP_ERR;
  }

  app->config.profile = spn_pkg_get_profile_or_default(&app->package, name);
  return SP_APP_CONTINUE;
}

// Get resolved package path from resolver (doesn't require builder init)
sp_str_t spn_cli_get_resolved_pkg_source(sp_str_t name) {
  spn_resolved_pkg_t* resolved = sp_str_ht_get(app.resolver.resolved, name);
  SP_ASSERT_FMT(resolved, "{:fg brightyellow} is not in this project", SP_FMT_STR(name));
  return sp_fs_join_path(spn.paths.source, resolved->pkg->name);
}

sp_app_result_t spn_cli_init(spn_cli_t* cli) {
  spn_cli_init_t* cmd = &cli->init;

  spn_app_t app = spn_app_init_and_write(
    spn.paths.cwd,
    sp_fs_get_stem(spn.paths.cwd),
    cmd->bare ? SPN_APP_INIT_BARE : SPN_APP_INIT_NORMAL
  );

  SP_LOG("Initialized project {:fg brightcyan}. Run {:fg brightyellow} to build.", SP_FMT_STR(app.package.name), SP_FMT_CSTR("spn build"));
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_root(spn_cli_t* cli) {
  sp_str_t help = spn_cli_usage(&cli->cmd);
  sp_log(help);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_list(spn_cli_t* cli) {
  sp_tui_begin_table(&spn.tui.table);
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Package"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Version"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Repo"));
  sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Author"));
  sp_tui_table_header_row(&spn.tui.table);

  sp_str_ht_for_kv(app.registry, it) {
    spn_pkg_t* package = spn_app_ensure_package(&app, (spn_pkg_req_t) {
      .name = sp_fs_get_stem(*it.val),
      .kind = SPN_PACKAGE_KIND_INDEX
    });

    sp_tui_table_next_row(&spn.tui.table);
    sp_tui_table_fmt(&spn.tui.table, "{:fg brightcyan}", SP_FMT_STR(package->name));
    sp_tui_table_str(&spn.tui.table, spn_semver_to_str(package->version));
    sp_tui_table_str(&spn.tui.table, sp_str_truncate(package->repo, 50, sp_str_lit("...")));
    sp_tui_table_str(&spn.tui.table, sp_str_truncate(package->author, 30, sp_str_lit("...")));
  }

  sp_tui_table_end(&spn.tui.table);
  sp_log(sp_tui_table_render(&spn.tui.table));
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_clean(spn_cli_t* cli) {
  spn_cli_clean_t* cmd = &cli->clean;

  // Create a minimal context for clean events
  spn_build_ctx_t ctx = SP_ZERO_INITIALIZE();
  ctx.name = sp_str_lit("package");

  sp_str_t build_dir = sp_fs_join_path(app.paths.dir, sp_str_lit("build"));

  if (sp_str_valid(cmd->profile)) {
    // Clean only the specified profile
    sp_str_t profile_dir = sp_fs_join_path(build_dir, cmd->profile);
    if (sp_fs_exists(profile_dir)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = profile_dir
      });
      sp_fs_remove_dir(profile_dir);
    }
  } else {
    // Clean the entire build directory
    if (sp_fs_exists(build_dir)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = build_dir
      });
      sp_fs_remove_dir(build_dir);
    }

    // Remove the lock file
    if (sp_fs_exists(app.paths.lock)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = app.paths.lock
      });
      sp_fs_remove_file(app.paths.lock);
    }
  }

  // Drain and render events
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(spn.events);
  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];
    sp_io_write_line(&spn.logger.err, spn_tui_render_build_event(event, spn.tui.info.max_name));
  }

  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_copy(spn_cli_t* cli) {
  spn_cli_copy_t* cmd = &cli->copy;

  sp_str_t destination = sp_fs_normalize_path(cmd->directory);
  sp_str_t to = sp_fs_join_path(spn.paths.cwd, destination);
  sp_fs_create_dir(to);

  sp_om_for(app.session.units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(app.session.units.packages, it);
    spn_build_ctx_t* ctx = &dep->ctx;

    sp_dyn_array(sp_os_dir_ent_t) entries = sp_fs_collect(ctx->paths.lib);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t* entry = entries + i;
      sp_fs_copy_file(
        entry->file_path,
        sp_fs_join_path(to, sp_fs_get_name(entry->file_path))
      );
    }
  }
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_ls(spn_cli_t* cli) {
  spn_cli_ls_t* cmd = &cli->ls;

  spn_app_resolve(&app);

  if (sp_str_valid(cmd->package)) {
    sp_str_t dir = spn_cli_get_resolved_pkg_source(cmd->package);
    sp_sh_ls(dir);
  }
  else {
    spn_pkg_dir_t kind = SPN_DIR_CACHE;
    if (sp_str_valid(cmd->dir)) {
      kind = spn_cache_dir_kind_from_str(cmd->dir);
    }

    sp_str_t dir = spn_cache_dir_kind_to_path(kind);
    sp_sh_ls(dir);
  }
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_which(spn_cli_t* cli) {
  sp_try(spn_cli_set_profile(&app, sp_str_lit("")));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_WHICH);
  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_manifest(spn_cli_t* cli) {
  spn_cli_manifest_t* cmd = &cli->manifest;

  spn_app_resolve(&app);

  spn_pkg_unit_t* dep = spn_session_find_pkg_or_assert(&app.session, cmd->package);

  sp_str_t path = dep->ctx.pkg->paths.manifest;
  sp_str_t manifest = sp_io_read_file(path);
  if (!sp_str_valid(manifest)) {
    SP_FATAL("Failed to read manifest at {:fg brightyellow}", SP_FMT_STR(path));
  }

  sp_log(manifest);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_graph(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config = (spn_app_config_t) {
    .force = command->force,
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .public = false,
        .test = false,
      }
    },
  };

  sp_try(spn_cli_set_profile(&app, command->profile));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RENDER_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_add(spn_cli_t* cli) {
  spn_cli_add_t* cmd = &cli->add;

  if (cmd->test && cmd->build) {
    SP_FATAL("cannot specify both {:fg yellow} and {:fg yellow}", SP_FMT_CSTR("--test"), SP_FMT_CSTR("--build"));
  }

  spn_visibility_t visibility = cmd->test  ? SPN_VISIBILITY_TEST
                              : cmd->build ? SPN_VISIBILITY_BUILD
                              :              SPN_VISIBILITY_PUBLIC;
  if (sp_ht_getp(app.package.deps, cmd->package)) {
    SP_FATAL("{:fg brightyellow} is already in your project", SP_FMT_STR(cmd->package));
  }
  spn_pkg_add_dep_latest(&app.package, cmd->package, visibility);
  spn_app_resolve_from_solver(&app);
  spn_app_update_lock_file(&app);
  spn_app_write_manifest(&app.package, app.package.paths.manifest);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_update(spn_cli_t* cli) {
  spn_cli_update_t* cmd = &cli->update;

  spn_pkg_req_t* existing = sp_ht_getp(app.package.deps, cmd->package);
  if (!existing) {
    SP_FATAL("package {:fg brightcyan} is not a dependency", SP_FMT_STR(cmd->package));
  }

  spn_pkg_add_dep_latest(&app.package, cmd->package, existing->visibility);
  spn_app_resolve_from_solver(&app);
  spn_app_update_lock_file(&app);
  spn_app_write_manifest(&app.package, app.package.paths.manifest);
  return SP_APP_QUIT;
}

sp_app_result_t spn_cli_tool_install(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_tool_uninstall(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_tool_run(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_tool(spn_cli_t* cli) {
  SPN_CLI_UNIMPLEMENTED();
}

sp_app_result_t spn_cli_generate(spn_cli_t* cli) {
  spn_cli_generate_t* command = &cli->generate;

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    SP_FATAL(
      "output path was specified, but no generator. try e.g.:\n  spn generate --path {} {:fg yellow}",
      SP_FMT_STR(command->path),
      SP_FMT_CSTR("--generator make")
    );
  }
  if (!sp_str_valid(command->generator)) command->generator = sp_str_lit("");
  if (!sp_str_valid(command->compiler)) command->compiler = sp_str_lit("gcc");

  if (!app.lock.some) {
    SP_FATAL("No lock file found. Run {:fg yellow} first.", SP_FMT_CSTR("spn build"));
  }

  sp_try(spn_cli_set_profile(&app, sp_str_lit("")));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_GENERATE);

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_test(spn_cli_t* cli) {
  spn_cli_test_t* command = &cli->test;

  app.config = (spn_app_config_t) {
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .public = true
      }
    }
  };

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN);

  sp_try_as(spn_cli_set_profile(&app, command->profile), SP_APP_ERR);

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config = (spn_app_config_t) {
    .force = command->force,
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .test = sp_str_empty(command->target) && !command->tests
      }
    },
  };

  sp_try(spn_cli_set_profile(&app, command->profile));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}

spn_cli_command_usage_t spn_cli() {
  spn_cli_t* cli = &spn.cli;
  static spn_cli_command_usage_t tools [] = {
    {
      .name = "install",
      .opts = {
        {
          .brief = "f",
          .name = "force",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Force reinstall even if already installed",
          .ptr = &spn.cli.tool.install.force
        },
        {
          .brief = "v",
          .name = "version",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Version to install",
          .placeholder = "VERSION",
          .ptr = &spn.cli.tool.install.version
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to install",
          .ptr = &spn.cli.tool.install.package
        }
      },
      .summary = "Install a package's binary targets to the PATH",
      .handler = spn_cli_tool_install
    },
    {
      .name = "uninstall",
      .opts = {
        {
          .brief = "f",
          .name = "force",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Force removal even if not installed by spn",
          .ptr = &spn.cli.tool.install.force
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to uninstall",
          .ptr = &spn.cli.tool.install.package
        }
      },
      .summary = "Uninstall a package's binary targets from the PATH",
      .handler = spn_cli_tool_uninstall
    },
    {
      .name = "run",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to run",
          .ptr = &spn.cli.tool.run.package
        },
        {
          .name = "command",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The command to run",
          .ptr = &spn.cli.tool.run.command
        }
      },
      .summary = "Run a binary from a package",
      .handler = spn_cli_tool_run
    },
    SPN_CLI_ARGS_DONE,
  };

  static spn_cli_command_usage_t commands [] = {
    {
      .name = "init",
      .handler = spn_cli_init,
      .summary = "Initialize a project in the current directory",
      .opts = {
        {
          .brief = "b",
          .name = "bare",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Create minimal project without sp dependency or main.c",
          .ptr = &spn.cli.init.bare
        }
      },
    },
    {
      .name = "add",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to add",
          .ptr = &spn.cli.add.package
        }
      },
      .opts = {
        {
          .brief = "t",
          .name = "test",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Add as a test dependency",
          .ptr = &spn.cli.add.test
        },
        {
          .brief = "b",
          .name = "build",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Add as a build dependency",
          .ptr = &spn.cli.add.build
        }
      },
      .summary = "Add the latest version of a package to the project",
      .handler = spn_cli_add
    },

    {
      .name = "build",
      .opts = {
        {
          .brief = "f",
          .name = "force",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Force build, even if it exists in store",
          .ptr = &spn.cli.build.force
        },
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Profile to use for building",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.build.profile
        },
        {
          .brief = "t",
          .name = "target",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Target to build",
          .placeholder = "TARGET",
          .ptr = &spn.cli.build.target
        },
        {
          .name = "tests",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Include test targets",
          .ptr = &spn.cli.build.tests
        }
      },
      .summary = "Build the project, including dependencies, from source",
      .handler = spn_cli_build
    },

    {
      .name = "test",
      .opts = {
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Profile to use for building",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.test.profile
        },
        {
          .brief = "t",
          .name = "target",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Test target to run",
          .placeholder = "TARGET",
          .ptr = &spn.cli.test.target
        }
      },
      .summary = "Build and run tests",
      .handler = spn_cli_test
    },

    {
      .name = "generate",
      .opts = {
        {
          .brief = "g",
          .name = "generator",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Generator type (raw, shell, make)",
          .placeholder = "TYPE",
          .ptr = &spn.cli.generate.generator
        },
        {
          .brief = "c",
          .name = "compiler",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Compiler to format flags for (gcc, clang, tcc)",
          .placeholder = "COMPILER",
          .ptr = &spn.cli.generate.compiler
        },
        {
          .brief = "p",
          .name = "path",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Output directory for generated file",
          .placeholder = "PATH",
          .ptr = &spn.cli.generate.path
        }
      },
      .summary = "Generate build system files with dependency flags",
      .handler = spn_cli_generate
    },
    {
      .name = "link",
      .handler = spn_cli_copy,
      .args = {
        {
          .name = "kind",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The link kind",
          .ptr = &spn.cli.copy.directory
        }
      },
      .summary = "Link or copy the binary outputs of your dependencies",
    },
    {
      .name = "update",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to update",
          .ptr = &spn.cli.update.package
        }
      },
      .summary = "Update an existing package to the latest version in the project",
      .handler = spn_cli_update
    },
    {
      .name = "list",
      .summary = "List all known packages in all registries",
      .handler = spn_cli_list
    },
    {
      .name = "which",
      .opts = {
        {
          .brief = "d",
          .name = "dir",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Which directory to show (store, include, lib, source, work, vendor)",
          .placeholder = "DIR",
          .ptr = &spn.cli.which.dir
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The package to show path for",
          .ptr = &spn.cli.which.package
        }
      },
      .summary = "Print the absolute path of a cache dir for a package",
      .handler = spn_cli_which
    },
    {
      .name = "ls",
      .opts = {
        {
          .brief = "d",
          .name = "dir",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Which directory to list (store, include, lib, source, work, vendor)",
          .placeholder = "DIR",
          .ptr = &spn.cli.ls.dir
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The package to list",
          .ptr = &spn.cli.ls.package
        }
      },
      .summary = "Run ls against a cache dir for a package (e.g. to see build output)",
      .handler = spn_cli_ls
    },
    {
      .name = "manifest",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package name",
          .ptr = &spn.cli.manifest.package
        }
      },
      .summary = "Print the full manifest source for a package",
      .handler = spn_cli_manifest
    },
    {
      .name = "graph",
      .opts = {
        {
          .brief = "o",
          .name = "output",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Output file path (stdout if not specified)",
          .placeholder = "FILE",
          .ptr = &spn.cli.graph.output
        },
        {
          .brief = "d",
          .name = "dirty",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Color nodes by dirtiness instead of type",
          .ptr = &spn.cli.graph.dirty
        }
      },
      .summary = "Output the build graph as mermaid",
      .handler = spn_cli_graph
    },
    {
      .name = "tool",
      .summary = "Run, install, and manage binaries defined by spn packages",
      .handler = spn_cli_tool,
      .commands = tools
    },
    {
      .name = "clean",
      .summary = "Remove build directory and lock file",
      .opts = {
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Only clean the specified profile",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.clean.profile
        }
      },
      .handler = spn_cli_clean
    },
    SPN_CLI_ARGS_DONE,
  };

  return (spn_cli_command_usage_t) {
    .name = "spn",
    .handler = spn_cli_root,
    .summary = "A package manager and build tool for modern C",
    .opts = {
      {
        .brief = "h",
        .name = "help",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Print help message",
        .ptr = &spn.cli.help
      },
      {
        .brief = "C",
        .name = "project-dir",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Specify the directory containing project file",
        .placeholder = "DIR",
        .ptr = &spn.cli.project_dir
      },
      {
        .brief = "f",
        .name = "file",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Specify the project file path",
        .placeholder = "FILE",
        .ptr = &spn.cli.project_file
      },
      {
        .brief = "o",
        .name = "output",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Output mode: interactive, noninteractive, quiet, none",
        .placeholder = "MODE",
        .ptr = &spn.cli.output
      },
      {
        .brief = "v",
        .name = "verbose",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Show verbose output",
        .ptr = &spn.cli.verbose
      },
      {
        .brief = "q",
        .name = "quiet",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Only show errors",
        .ptr = &spn.cli.quiet
      }
    },
    .commands = commands
  };
}
