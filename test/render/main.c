#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_test.h"

#include "render.h"

static s32 sort_cells_by_name(const void* a, const void* b) {
  const sp_fs_entry_t* lhs = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* rhs = (const sp_fs_entry_t*)b;
  return sp_str_sort_kernel_alphabetical(&lhs->name, &rhs->name);
}

static void view_cell(sp_io_writer_t* io, sp_mem_t mem, sp_fs_entry_t* cell) {
  sp_str_t exit_code = sp_str_trim_right(test_read_file(mem, sp_fs_join_path(mem, cell->path, sp_str_lit("exit"))));
  sp_str_t err = test_read_file(mem, sp_fs_join_path(mem, cell->path, sp_str_lit("stderr")));
  sp_str_t out = test_read_file(mem, sp_fs_join_path(mem, cell->path, sp_str_lit("stdout")));

  sp_fmt_io(io, "{.bold} {.gray}\n", sp_fmt_str(cell->name), sp_fmt_str(sp_fmt(mem, "(exit {})", sp_fmt_str(exit_code)).value));
  sp_io_write_str(io, err, SP_NULLPTR);
  if (!sp_str_empty(out)) {
    sp_fmt_io(io, "{.gray}\n", sp_fmt_cstr("stdout:"));
    sp_io_write_str(io, out, SP_NULLPTR);
  }
  sp_io_write_c8(io, '\n');
}

static s32 view(void) {
  sp_mem_t mem = sp_mem_os_new();
  sp_io_stream_writer_t w = sp_zero;
  sp_io_stream_writer_from_fd(&w, sp_sys_stdout, SP_IO_CLOSE_MODE_NONE);

  sp_str_t current = render_out_path(mem, "current");
  if (!sp_fs_exists(current)) {
    sp_fmt_io(&w.base, "no capture at {}; run render first\n", sp_fmt_str(current));
    return 1;
  }

  sp_da(sp_fs_entry_t) cells = sp_zero;
  sp_fs_collect(mem, current, &cells);
  sp_da_sort(cells, sort_cells_by_name);
  sp_da_for(cells, it) {
    view_cell(&w.base, mem, &cells[it]);
  }
  return 0;
}

static s32 diff(void) {
  sp_mem_t mem = sp_mem_os_new();
  sp_io_stream_writer_t w = sp_zero;
  sp_io_stream_writer_from_fd(&w, sp_sys_stdout, SP_IO_CLOSE_MODE_NONE);

  if (!sp_fs_exists(render_out_path(mem, "previous"))) {
    sp_fmt_io(&w.base, "no previous capture at {}; run render twice\n", sp_fmt_str(render_out_path(mem, "previous")));
    return 1;
  }

  sp_ps_config_t config = {
    .command = sp_str_lit("git"),
    .cwd = render_out_root(mem),
  };
  sp_ps_config_add_arg(mem, &config, sp_str_lit("diff"));
  if (sp_sys_is_tty(sp_sys_stdout)) {
    sp_ps_config_add_arg(mem, &config, sp_str_lit("--color"));
  }
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--no-index"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("previous"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("current"));

  sp_ps_output_t output = sp_ps_run(mem, config);
  sp_io_write_str(&w.base, output.out, SP_NULLPTR);

  return output.status.exit_code > 1 ? 1 : 0;
}

static void rotate(void) {
  sp_mem_t mem = sp_mem_os_new();
  sp_str_t current = render_out_path(mem, "current");
  sp_str_t previous = render_out_path(mem, "previous");
  if (!sp_fs_exists(current)) {
    return;
  }
  if (sp_fs_exists(previous)) {
    sp_fs_remove_dir(previous);
  }
  sp_sys_fd_t cwd = sp_sys_get_root(0);
  sp_assert(!sp_sys_rename_s(cwd, current, cwd, previous));
}

s32 main(s32 argc, const c8** argv) {
  if (argc > 1 && sp_cstr_equal(argv[1], "view")) {
    return view();
  }
  if (argc > 1 && sp_cstr_equal(argv[1], "diff")) {
    return diff();
  }

  rotate();

  bool jobs = false;
  sp_for(it, (u32)argc) {
    if (sp_str_starts_with(sp_cstr_as_str(argv[it]), sp_str_lit("--jobs"))) {
      jobs = true;
    }
  }
  if (jobs) {
    return sp_test_main(argc, argv, SP_NULLPTR);
  }

  sp_mem_t mem = sp_mem_os_new();
  const c8** args = sp_alloc_n(mem, const c8*, argc + 1);
  sp_for(it, (u32)argc) {
    args[it] = argv[it];
  }
  args[argc] = "--jobs=0";
  return sp_test_main(argc + 1, args, SP_NULLPTR);
}
