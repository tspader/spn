#define SP_OS_BACKEND_NATIVE
#define SP_IMPLEMENTATION
#include "sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

#include "fnmatch.h"

/////////////
// TESTING //
/////////////

#define SPN_TEST_BUILD_KIND(X) \
  X(SPN_TEST_BUILD_KIND_ALL,    "all") \
  X(SPN_TEST_BUILD_KIND_SHARED, "shared") \
  X(SPN_TEST_BUILD_KIND_SOURCE, "source") \
  X(SPN_TEST_BUILD_KIND_STATIC, "static")

typedef enum {
  SPN_TEST_BUILD_KIND(SP_X_NAMED_ENUM_DEFINE)
  SPN_TEST_BUILD_KIND_COUNT,
} spn_test_build_kind_t;

#define SPN_TEST_BUILD_MODE(X) \
  X(SPN_TEST_BUILD_MODE_ALL,     "all") \
  X(SPN_TEST_BUILD_MODE_DEBUG,   "debug") \
  X(SPN_TEST_BUILD_MODE_RELEASE, "release")

typedef enum {
  SPN_TEST_BUILD_MODE(SP_X_NAMED_ENUM_DEFINE)
  SPN_TEST_BUILD_MODE_COUNT,
} spn_test_build_mode_t;

#define SPN_TEST_LANGUAGE(X) \
  X(SPN_TEST_LANGUAGE_C,   "c") \
  X(SPN_TEST_LANGUAGE_CPP, "cpp")

typedef enum {
  SPN_TEST_LANGUAGE(SP_X_NAMED_ENUM_DEFINE)
} spn_test_language_t;

#define SPN_TEST_STATUS(X) \
  X(SPN_TEST_STATUS_PENDING,        "pending") \
  X(SPN_TEST_STATUS_BUILDING_DEPS,  "building") \
  X(SPN_TEST_STATUS_COMPILING,      "compiling") \
  X(SPN_TEST_STATUS_SUCCESS,        "success") \
  X(SPN_TEST_STATUS_FAILED_DEPS,    "failed") \
  X(SPN_TEST_STATUS_FAILED_COMPILE, "failed") \
  X(SPN_TEST_STATUS_DRY_RUN,        "dry")

typedef enum {
  SPN_TEST_STATUS(SP_X_NAMED_ENUM_DEFINE)
  SPN_TEST_STATUS_COUNT,
} spn_test_status_t;


typedef u64 spn_timestamp_t;

typedef struct {
  sp_str_t status;
  sp_str_t name;
  sp_str_t kind;
  sp_str_t mode;
  sp_str_t build;
  sp_str_t copy;
  sp_str_t cc;
} spn_test_summary_row_t;

typedef struct {
  sp_str_t path;
  sp_str_t name;
  struct {
    sp_str_t c;
    sp_str_t cpp;
  } main;
} spn_test_example_info_t;

typedef struct {
  sp_str_t raw;
  sp_str_t log_path;
  s32 code;
  u64 duration_ms;
  bool executed;
} spn_test_command_t;

typedef struct {
  sp_str_t name;
  sp_str_t project_file;
  sp_str_t main;
  struct {
    struct {
      sp_str_t relative;
      sp_str_t absolute;
    } dir;
    sp_str_t logs;
    sp_str_t executable;
  } output;

  spn_test_build_kind_t kind;
  spn_test_build_mode_t mode;
  spn_test_status_t status;
  spn_test_status_t tui_status;
  sp_str_t error;
  spn_test_language_t language;

  struct {
    spn_test_command_t build;
    spn_test_command_t copy;
    spn_test_command_t print;
    spn_test_command_t compile;
  } commands;

  sp_thread_t thread;
  sp_mutex_t mutex;
} spn_test_descriptor_t;

typedef struct {
  sp_str_t name;
  sp_dyn_array(u32) tests;
  u32 index;
} spn_test_queue_t;

typedef struct {
  struct {
    const c8* kind;
    const c8* mode;
    const c8** args;
    s32 num_args;
    u32 jobs;
    bool dry_run;
    bool no_interactive;
  } cli;

  struct {
    sp_str_t build;
    sp_str_t bin;
    sp_str_t spn;
    sp_str_t repo;
    sp_str_t asset;
    sp_str_t recipes;
    sp_str_t examples;
  } paths;

  spn_test_build_kind_t kind;
  spn_test_build_mode_t mode;
  sp_dyn_array(sp_str_t) packages;

  sp_dyn_array(spn_test_descriptor_t) tests;
  sp_dyn_array(spn_test_queue_t) queues;
  u32 num_skipped;

  struct {
    u32 name;
    u32 kind;
    u32 mode;
    u32 status;
    u32 time;
    u32 copy;
    u32 cc;
  } pad;
} spn_test_app_t;

spn_test_app_t app;

spn_test_build_kind_t spn_test_build_kind_from_str(sp_str_t str);
spn_test_build_mode_t spn_test_build_mode_from_str(sp_str_t str);
sp_str_t              spn_test_build_kind_to_str(spn_test_build_kind_t kind);
sp_str_t              spn_test_build_mode_to_str(spn_test_build_mode_t mode);
sp_str_t              spn_test_status_to_str(spn_test_status_t status);
sp_str_t              sp_os_read_file(sp_str_t file_path);
void spn_test_tui_print_summary_header();
sp_str_t spn_test_tui_format_row(spn_test_summary_row_t row);
void spn_tui_update_noninteractive(void);
void spn_tui_update_interactive(void);
bool spn_test_is_all_done(void);
void spn_test_tui_update(void);
s32 spn_test_qsort_kernel(const void* va, const void* vb);

spn_test_build_kind_t spn_test_build_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "shared")) return SPN_TEST_BUILD_KIND_SHARED;
  else if (sp_str_equal_cstr(str, "static")) return SPN_TEST_BUILD_KIND_STATIC;
  else if (sp_str_equal_cstr(str, "source")) return SPN_TEST_BUILD_KIND_SOURCE;
  else                                        return SPN_TEST_BUILD_KIND_ALL;
}

spn_test_build_mode_t spn_test_build_mode_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "debug"))   return SPN_TEST_BUILD_MODE_DEBUG;
  else if (sp_str_equal_cstr(str, "release")) return SPN_TEST_BUILD_MODE_RELEASE;
  else                                        return SPN_TEST_BUILD_MODE_ALL;
}

sp_str_t spn_test_status_to_color(spn_test_status_t status, sp_str_t padded) {
  switch (status) {
    case SPN_TEST_STATUS_SUCCESS:        return sp_format("{:fg brightgreen}",  SP_FMT_STR(padded));
    case SPN_TEST_STATUS_FAILED_DEPS:    return sp_format("{:fg brightred}",    SP_FMT_STR(padded));
    case SPN_TEST_STATUS_FAILED_COMPILE: return sp_format("{:fg brightred}",    SP_FMT_STR(padded));
    case SPN_TEST_STATUS_DRY_RUN:        return sp_format("{:fg brightgreen}", SP_FMT_STR(padded));
    case SPN_TEST_STATUS_PENDING:        return sp_format("{:fg brightyellow}", SP_FMT_STR(padded));
    case SPN_TEST_STATUS_BUILDING_DEPS:  return sp_format("{:fg brightcyan}",   SP_FMT_STR(padded));
    case SPN_TEST_STATUS_COMPILING:      return sp_format("{:fg brightcyan}",   SP_FMT_STR(padded));
    default:                             return sp_format("{:fg white}",  SP_FMT_STR(padded));
  }
}

sp_str_t spn_test_build_kind_to_str(spn_test_build_kind_t kind) {
  switch (kind) {
    SPN_TEST_BUILD_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING)
    default: SP_UNREACHABLE_RETURN(SP_LIT(""));
  }
}

sp_str_t spn_test_build_mode_to_str(spn_test_build_mode_t mode) {
  switch (mode) {
    SPN_TEST_BUILD_MODE(SP_X_NAMED_ENUM_CASE_TO_STRING)
    default: SP_UNREACHABLE_RETURN(SP_LIT(""));
  }
}

sp_str_t spn_test_status_to_str(spn_test_status_t status) {
  switch (status) {
    SPN_TEST_STATUS(SP_X_NAMED_ENUM_CASE_TO_STRING)
    default: SP_UNREACHABLE_RETURN(SP_LIT(""));
  }
}

s32 spn_test_qsort_kernel(const void* va, const void* vb) {
  spn_test_descriptor_t* a = (spn_test_descriptor_t*)va;
  spn_test_descriptor_t* b = (spn_test_descriptor_t*)vb;

  s32 cmp = sp_str_compare_alphabetical(a->name, b->name);
  if (cmp != SP_QSORT_EQUAL) return cmp;

  if (a->mode > b->mode) return SP_QSORT_A_FIRST;
  if (a->mode < b->mode) return SP_QSORT_B_FIRST;

  if (a->kind > b->kind) return SP_QSORT_A_FIRST;
  if (a->kind < b->kind) return SP_QSORT_B_FIRST;

  if (a->language > b->language) return SP_QSORT_A_FIRST;
  if (a->language < b->language) return SP_QSORT_B_FIRST;

  SP_UNREACHABLE_RETURN(SP_QSORT_EQUAL);
};

spn_timestamp_t spn_time_now(void) {
  sp_os_date_time_t dt = sp_os_get_date_time();
  spn_timestamp_t value = 0;
  value += (u64)dt.millisecond;
  value += (u64)dt.second * 1000ULL;
  value += (u64)dt.minute * 60000ULL;
  value += (u64)dt.hour * 3600000ULL;
  value += (u64)dt.day * 86400000ULL;
  value += (u64)dt.month * 31ULL * 86400000ULL;
  value += (u64)dt.year * 372ULL * 86400000ULL;
  return value;
}

void spn_test_tui_print_summary_header() {
  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append_c8(&builder, '{');
  sp_str_builder_append_fmt(&builder, ":fg {}", SP_FMT_CSTR("brightblack"));
  sp_str_builder_append_c8(&builder, '}');
  sp_str_t placeholder = sp_str_builder_write(&builder);

  sp_dyn_array(sp_str_t) placeholders = SP_NULLPTR;
  u32 num_placeholders = sizeof(spn_test_summary_row_t) / sizeof(sp_str_t);
  for (u32 i = 0; i < num_placeholders; i++) {
    sp_dyn_array_push(placeholders, placeholder);
  }

  spn_test_summary_row_t header = {
    .status = sp_str_pad(sp_str_lit("status"), app.pad.status),
    .name = sp_str_pad(sp_str_lit("example"), app.pad.name),
    .kind = sp_str_pad(sp_str_lit("kind"), app.pad.kind),
    .mode = sp_str_pad(sp_str_lit("mode"), app.pad.mode),
    .build = sp_str_pad(sp_str_lit("spn build"), app.pad.time),
    .copy = sp_str_pad(sp_str_lit("spn copy"), app.pad.copy),
    .cc = sp_str_pad(sp_str_lit("compile"), app.pad.cc),
  };

  sp_log(sp_format_str(
    sp_str_join_n(placeholders, num_placeholders, sp_str_lit(" ")),
    SP_FMT_STR(header.status),
    SP_FMT_STR(header.name),
    SP_FMT_STR(header.kind),
    SP_FMT_STR(header.mode),
    SP_FMT_STR(header.build),
    SP_FMT_STR(header.copy),
    SP_FMT_STR(header.cc)
  ));
}

sp_str_t spn_test_tui_format_row(spn_test_summary_row_t row) {
  return sp_format(
    "{} {} {} {} {} {} {}",
    SP_FMT_STR(row.status),
    SP_FMT_STR(row.name),
    SP_FMT_STR(row.kind),
    SP_FMT_STR(row.mode),
    SP_FMT_STR(row.build),
    SP_FMT_STR(row.copy),
    SP_FMT_STR(row.cc)
  );
}

u64 spn_time_elapsed_ms(spn_timestamp_t start, spn_timestamp_t end) {
  if (end< start) return 0;
  return end - start;
}

sp_str_t sp_os_read_file(sp_str_t path) {
  s32 handle = open(sp_str_to_cstr(path), O_RDONLY);
  if (handle < 0) {
    return SP_LIT("");
  }

  sp_str_builder_t builder = SP_ZERO_STRUCT(sp_str_builder_t);
  c8 buffer[1024];

  while (true) {
    s32 bytes_read = (s32)read(handle, buffer, sizeof(buffer));
    if (bytes_read <= 0) {
      break;
    }

    sp_str_t chunk = sp_str(buffer, (u32)bytes_read);
    sp_str_builder_append(&builder, chunk);
  }

  close(handle);
  return sp_str_builder_write(&builder);
}

spn_test_build_kind_t* spn_test_collect_kinds_for_example(sp_str_t example, spn_test_build_kind_t filter) {
  sp_dyn_array(spn_test_build_kind_t) kinds = SP_NULLPTR;

  sp_str_t shared_config = sp_os_join_path(example, SP_LIT("shared.lua"));
  if (sp_os_does_path_exist(shared_config)) {
    if (filter == SPN_TEST_BUILD_KIND_ALL || filter == SPN_TEST_BUILD_KIND_SHARED) {
      sp_dyn_array_push(kinds, SPN_TEST_BUILD_KIND_SHARED);
    }
  }

  sp_str_t static_config = sp_os_join_path(example, SP_LIT("static.lua"));
  if (sp_os_does_path_exist(static_config)) {
    if (filter == SPN_TEST_BUILD_KIND_ALL || filter == SPN_TEST_BUILD_KIND_STATIC) {
      sp_dyn_array_push(kinds, SPN_TEST_BUILD_KIND_STATIC);
    }
  }

  sp_str_t source_config = sp_os_join_path(example, SP_LIT("source.lua"));
  if (sp_os_does_path_exist(source_config)) {
    if (filter == SPN_TEST_BUILD_KIND_ALL || filter == SPN_TEST_BUILD_KIND_SOURCE) {
      sp_dyn_array_push(kinds, SPN_TEST_BUILD_KIND_SOURCE);
    }
  }

  return kinds;
}

sp_str_t spn_project_file_for_kind(sp_str_t example_dir, spn_test_build_kind_t kind) {
  switch (kind) {
    case SPN_TEST_BUILD_KIND_SHARED: {
      sp_str_t candidate = sp_os_join_path(example_dir, SP_LIT("shared.lua"));
      if (sp_os_does_path_exist(candidate)) {
        return candidate;
      }
      return sp_os_join_path(example_dir, SP_LIT("spn.lua"));
    }
    case SPN_TEST_BUILD_KIND_STATIC: {
      sp_str_t candidate = sp_os_join_path(example_dir, SP_LIT("static.lua"));
      if (sp_os_does_path_exist(candidate)) {
        return candidate;
      }
      return sp_os_join_path(example_dir, SP_LIT("spn.lua"));
    }
    case SPN_TEST_BUILD_KIND_SOURCE: {
      sp_str_t candidate = sp_os_join_path(example_dir, SP_LIT("source.lua"));
      if (sp_os_does_path_exist(candidate)) {
        return candidate;
      }
      return sp_os_join_path(example_dir, SP_LIT("spn.lua"));
    }
    default: {
      return sp_os_join_path(example_dir, SP_LIT("spn.lua"));
    }
  }
}

spn_test_command_t spn_make_command(sp_str_t raw, sp_str_t log_path) {
  return SP_RVAL(spn_test_command_t) {
    .raw = raw,
    .log_path = log_path,
    .code = 0,
    .duration_ms = 0,
    .executed = false,
  };
}

void spn_prepare_directories(spn_test_descriptor_t* test) {
  sp_os_create_directory(sp_os_join_path(app.paths.repo, SP_LIT("build")));
  sp_os_create_directory(sp_os_join_path(app.paths.repo, SP_LIT("build/examples")));
  sp_os_create_directory(sp_os_join_path(app.paths.repo, sp_format("build/examples/{}", SP_FMT_STR(test->name))));

  sp_os_create_directory(sp_os_join_path(app.paths.repo, sp_format("build/examples/{}/{}", SP_FMT_STR(test->name), SP_FMT_STR(spn_test_build_mode_to_str(test->mode)))));
  sp_os_create_directory(test->output.dir.absolute);
  sp_os_create_directory(test->output.logs);
}

s32 spn_run_command(spn_test_command_t* command) {
  if (!command->raw.len) {
    return 0;
  }

  command->executed = true;
  spn_timestamp_t start = spn_time_now();
  sp_str_t quoted_log = sp_format("\"{}\"", SP_FMT_STR(command->log_path));
  sp_str_t wrapped = sp_format("{} > {} 2>&1", SP_FMT_STR(command->raw), SP_FMT_STR(quoted_log));
  command->code = system(sp_str_to_cstr(wrapped));
  spn_timestamp_t end = spn_time_now();
  command->duration_ms = spn_time_elapsed_ms(start, end);
  return command->code;
}

bool spn_test_is_terminal(spn_test_descriptor_t* test) {
  return test->status == SPN_TEST_STATUS_SUCCESS ||
         test->status == SPN_TEST_STATUS_FAILED_DEPS ||
         test->status == SPN_TEST_STATUS_FAILED_COMPILE;
}

bool spn_test_is_running(spn_test_descriptor_t* test) {
  return test->status == SPN_TEST_STATUS_BUILDING_DEPS ||
         test->status == SPN_TEST_STATUS_COMPILING;
}

bool spn_test_queue_is_running(spn_test_queue_t* queue) {
  if (queue->index >= sp_dyn_array_size(queue->tests)) return false;
  u32 test_idx = queue->tests[queue->index];
  spn_test_descriptor_t* test = &app.tests[test_idx];
  sp_mutex_lock(&test->mutex);
  bool is_running = spn_test_is_running(test);
  sp_mutex_unlock(&test->mutex);
  return is_running;
}

bool spn_test_queue_is_done(spn_test_queue_t* queue) {
  if (sp_dyn_array_size(queue->tests) == 0) return true;
  if (queue->index < sp_dyn_array_size(queue->tests)) return false;
  if (queue->index == 0) return false;
  u32 test_idx = queue->tests[sp_dyn_array_size(queue->tests) - 1];
  spn_test_descriptor_t* test = &app.tests[test_idx];
  sp_mutex_lock(&test->mutex);
  bool is_terminal = spn_test_is_terminal(test);
  sp_mutex_unlock(&test->mutex);
  return is_terminal;
}

bool spn_test_are_queues_done(void) {
  sp_dyn_array_for(app.queues, index) {
    if (!spn_test_queue_is_done(&app.queues[index])) {
      return false;
    }
  }
  return true;
}

u32 spn_test_count_running_queues(void) {
  u32 count = 0;
  sp_dyn_array_for(app.queues, index) {
    if (spn_test_queue_is_running(&app.queues[index])) {
      count++;
    }
  }
  return count;
}

void spn_test_queue_advance(spn_test_queue_t* queue) {
  if (queue->index >= sp_dyn_array_size(queue->tests)) return;
  u32 test_idx = queue->tests[queue->index];
  spn_test_descriptor_t* test = &app.tests[test_idx];
  sp_mutex_lock(&test->mutex);
  bool is_terminal = spn_test_is_terminal(test);
  sp_mutex_unlock(&test->mutex);
  if (is_terminal) {
    queue->index++;
  }
}

bool spn_test_is_all_done(void) {
  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;

    sp_mutex_lock(&test->mutex);
    bool is_terminal = spn_test_is_terminal(test);
    sp_mutex_unlock(&test->mutex);

    if (!is_terminal) {
      return false;
    }
  }
  return true;
}

void spn_test_tui_update(void) {
  if (app.cli.no_interactive) {
    spn_tui_update_noninteractive();
  } else {
    spn_tui_update_interactive();
  }
}

void spn_test_set_status(spn_test_descriptor_t* test, spn_test_status_t status) {
  sp_mutex_lock(&test->mutex);
  test->status = status;
  sp_mutex_unlock(&test->mutex);
}

s32 spn_test_build_async(void* userdata) {
  spn_test_descriptor_t* test = (spn_test_descriptor_t*)userdata;

  spn_test_set_status(test, SPN_TEST_STATUS_BUILDING_DEPS);

  spn_prepare_directories(test);
  if (spn_run_command(&test->commands.build) != 0) {
    spn_test_set_status(test, SPN_TEST_STATUS_FAILED_DEPS);
    sp_mutex_lock(&test->mutex);
    test->error = SP_LIT("spn build failed");
    sp_mutex_unlock(&test->mutex);
    return 1;
  }

  if (test->commands.copy.raw.len) {
    if (spn_run_command(&test->commands.copy) != 0) {
      spn_test_set_status(test, SPN_TEST_STATUS_FAILED_DEPS);
      sp_mutex_lock(&test->mutex);
      test->error = SP_LIT("spn copy failed");
      sp_mutex_unlock(&test->mutex);
      return 1;
    }
  }

  spn_test_set_status(test, SPN_TEST_STATUS_COMPILING);
  if (spn_run_command(&test->commands.compile) != 0) {
    spn_test_set_status(test, SPN_TEST_STATUS_FAILED_COMPILE);
    sp_mutex_lock(&test->mutex);
    test->error = SP_LIT("gcc compilation failed");
    sp_mutex_unlock(&test->mutex);
    return 1;
  }

  spn_test_set_status(test, SPN_TEST_STATUS_SUCCESS);
  return 0;
}

void spn_log_failures(void) {
  bool printed_header = false;
  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;
    if (test->status == SPN_TEST_STATUS_FAILED_DEPS || test->status == SPN_TEST_STATUS_FAILED_COMPILE) {
      if (!printed_header) {
        sp_log(SP_LIT(""));
        sp_log(sp_format("{:fg red} failures", SP_FMT_CSTR("!")));
        printed_header = true;
      }

      sp_log(sp_format("{:fg red} {}", SP_FMT_CSTR("-"), SP_FMT_STR(test->name)));

      if (test->commands.build.executed) {
        sp_log(sp_format("  {:fg brightblack} $ {}", SP_FMT_CSTR("build"), SP_FMT_STR(test->commands.build.raw)));
        sp_str_t build_log = sp_os_read_file(test->commands.build.log_path);
        if (build_log.len) {
          sp_log(sp_format("  {:fg white} {}", SP_FMT_CSTR("build"), SP_FMT_STR(build_log)));
        }
      }

      if (test->commands.copy.executed) {
        sp_log(sp_format("  {:fg brightblack} $ {}", SP_FMT_CSTR("copy"), SP_FMT_STR(test->commands.copy.raw)));
        sp_str_t copy_log = sp_os_read_file(test->commands.copy.log_path);
        if (copy_log.len) {
          sp_log(sp_format("  {:fg white} {}", SP_FMT_CSTR("copy"), SP_FMT_STR(copy_log)));
        }
      }

      if (test->commands.compile.executed) {
        sp_log(sp_format("  {:fg brightblack} $ {}", SP_FMT_CSTR("compile"), SP_FMT_STR(test->commands.compile.raw)));
        sp_str_t compile_log = sp_os_read_file(test->commands.compile.log_path);
        if (compile_log.len) {
          sp_log(sp_format("  {:fg white} {}", SP_FMT_CSTR("compile"), SP_FMT_STR(compile_log)));
        }
      }
    }
  }
}

void sp_tui_print(sp_str_t str) {
  printf("%s", sp_str_to_cstr(str));
}

void sp_tui_up(u32 n) {
  sp_str_t command = sp_format("\033[{}A", SP_FMT_U32(n));
  sp_tui_print(command);
}

void sp_tui_down(u32 n) {
  sp_str_t command = sp_format("\033[{}B", SP_FMT_U32(n));
  sp_tui_print(command);
}

void sp_tui_clear_line(void) {
  sp_tui_print(SP_LIT("\033[2K"));
}

void sp_tui_home(void) {
  sp_tui_print(SP_LIT("\r"));
}

void sp_tui_flush(void) {
  fflush(stdout);
}

void spn_tui_update_interactive(void) {
  u32 num_tests = sp_dyn_array_size(app.tests);
  if (num_tests == 0) return;

  sp_tui_up(num_tests);

  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;

    sp_mutex_lock(&test->mutex);
    spn_test_status_t status = test->status;
    sp_mutex_unlock(&test->mutex);

    sp_tui_home();
    sp_tui_clear_line();


    sp_str_t row = spn_test_tui_format_row((spn_test_summary_row_t) {
      .status = spn_test_status_to_color(status, sp_str_pad(spn_test_status_to_str(status), app.pad.status)),
      .name = sp_str_pad(test->name, app.pad.name),
      .kind = sp_str_pad(spn_test_build_kind_to_str(test->kind), app.pad.kind),
      .mode = sp_str_pad(spn_test_build_mode_to_str(test->mode), app.pad.mode),
    });
    sp_tui_print(row);
    sp_tui_print(SP_LIT("\n"));
  }

  sp_tui_flush();
}

void spn_tui_update_noninteractive(void) {
  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;

    sp_mutex_lock(&test->mutex);
    spn_test_status_t status = test->status;
    sp_mutex_unlock(&test->mutex);

    if (test->tui_status != status) {
      sp_str_t row = spn_test_tui_format_row((spn_test_summary_row_t) {
        .status = spn_test_status_to_color(status, sp_str_pad(spn_test_status_to_str(status), app.pad.status)),
        .name = sp_str_pad(test->name, app.pad.name),
        .kind = sp_str_pad(spn_test_build_kind_to_str(test->kind), app.pad.kind),
        .mode = sp_str_pad(spn_test_build_mode_to_str(test->mode), app.pad.mode),
      });
      sp_log(row);

      test->tui_status = status;
    }
  }
}

void spn_print_summary(void) {
  if (sp_dyn_array_size(app.tests) == 0) {
    sp_log(sp_format("{:fg yellow} no tests to run", SP_FMT_CSTR("!")));
    return;
  }

  sp_log(sp_str_lit(""));

  spn_test_tui_print_summary_header();

  u32 passed = 0;
  u32 failed = 0;
  u32 skipped = 0;

  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;

    sp_str_t row = spn_test_tui_format_row((spn_test_summary_row_t) {
      .status = spn_test_status_to_color(test->status, sp_str_pad(spn_test_status_to_str(test->status), app.pad.status)),
      .name = sp_str_pad(test->name, app.pad.name),
      .kind = sp_str_pad(spn_test_build_kind_to_str(test->kind), app.pad.kind),
      .mode = sp_str_pad(spn_test_build_mode_to_str(test->mode), app.pad.mode),
      .build = sp_str_pad(sp_format("{}ms", SP_FMT_U32(test->commands.build.duration_ms)), app.pad.time),
      .copy = sp_str_pad(sp_format("{}ms", SP_FMT_U32(test->commands.copy.duration_ms)), app.pad.copy),
      .cc = sp_str_pad(sp_format("{}ms", SP_FMT_U32(test->commands.compile.duration_ms)), app.pad.cc),
    });
    sp_log(row);


    switch (test->status) {
      case SPN_TEST_STATUS_SUCCESS: { passed++; break; }
      case SPN_TEST_STATUS_FAILED_DEPS:
      case SPN_TEST_STATUS_FAILED_COMPILE: { failed++; break; }
      default: { break; }
    }
  }

  sp_log(SP_LIT(""));
  if (failed) {
    sp_log(sp_format("{:fg brightgreen} passed {:fg red} failed", SP_FMT_U32(passed), SP_FMT_U32(failed)));
  }
  else {
    sp_log(sp_format("{:fg green} passed", SP_FMT_U32(passed)));
  }
}

s32 main(s32 num_args, const c8** args) {
  sp_init_default();

  app = SP_ZERO_STRUCT(spn_test_app_t);
  sp_str_t exe_dir = sp_os_get_executable_path();
  app.paths.bin = exe_dir;
  app.paths.build = sp_os_parent_path(exe_dir);
  app.paths.repo = sp_os_parent_path(app.paths.build);
  app.paths.spn = sp_os_join_path(app.paths.bin, SP_LIT("spn"));
  app.paths.examples = sp_os_join_path(app.paths.repo, SP_LIT("examples"));
  app.paths.asset = sp_os_join_path(app.paths.repo, SP_LIT("asset"));
  app.paths.recipes = sp_os_join_path(app.paths.asset, SP_LIT("recipes"));

  struct argparse parser = {0};
  argparse_init(
    &parser,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_STRING('k', "kind",    &app.cli.kind,    "build kind: shared, static, source, all", SP_NULLPTR),
      OPT_STRING('m', "mode",    &app.cli.mode,    "build mode: debug, release, all", SP_NULLPTR),
      OPT_INTEGER('j', "jobs", &app.cli.jobs, "number of parallel jobs (default: 1)", SP_NULLPTR),
      OPT_BOOLEAN('d', "dry-run", &app.cli.dry_run, "list tests without building", SP_NULLPTR),
      OPT_BOOLEAN(0, "no-interactive", &app.cli.no_interactive, "disable interactive TUI", SP_NULLPTR),
      OPT_END(),
    },
    (const c8* const []) {
      "spn-test [package1 package2 ...]",
      SP_NULLPTR
    },
    0
  );

  app.cli.num_args = argparse_parse(&parser, num_args, args);
  app.cli.args = args;

  app.cli.kind = app.cli.kind ? app.cli.kind : "all";
  app.kind = spn_test_build_kind_from_str(sp_str_view(app.cli.kind));

  app.cli.mode = app.cli.mode ? app.cli.mode : "all";
  app.mode = spn_test_build_mode_from_str(sp_str_view(app.cli.mode));

  for (s32 i = 0; i < app.cli.num_args; i++) {
    sp_dyn_array_push(app.packages, sp_str_view(app.cli.args[i]));
  }

  if (app.cli.jobs == 0) {
    app.cli.jobs = 1;
  }

  for (u32 i = 0; i < SPN_TEST_STATUS_COUNT; i++) {
    sp_str_t str = spn_test_status_to_str(i);
    app.pad.status = SP_MAX(app.pad.status, str.len);
  }
  for (u32 i = 0; i < SPN_TEST_BUILD_MODE_COUNT; i++) {
    sp_str_t str = spn_test_build_mode_to_str(i);
    app.pad.mode = SP_MAX(app.pad.mode, str.len);
  }
  for (u32 i = 0; i < SPN_TEST_BUILD_KIND_COUNT; i++) {
    sp_str_t str = spn_test_build_kind_to_str(i);
    app.pad.kind = SP_MAX(app.pad.kind, str.len);
  }

  app.pad.name = sp_cstr_len("example");
  app.pad.time = 10;
  app.pad.copy = 10;
  app.pad.cc = 10;

  sp_os_directory_entry_list_t entries = sp_os_scan_directory(app.paths.examples);
  for (u32 i = 0; i < entries.count; i++) {
    sp_os_directory_entry_t* entry = entries.data + i;
    if (!sp_os_is_directory(entry->file_path)) continue;
    if (sp_str_empty(entry->file_name)) continue;
    if (sp_str_at(entry->file_name, 0) == '.') continue;

    if (sp_dyn_array_size(app.packages) > 0) {
      bool found = false;
      sp_dyn_array_for(app.packages, j) {
        sp_str_t package = app.packages[j];
        if (sp_str_equal(entry->file_name, package)) {
          found = true;
          break;
        } else if (!fnmatch(sp_str_to_cstr(package), sp_str_to_cstr(entry->file_name), FNM_CASEFOLD)) {
          found = true;
          break;
        }
      }
      if (!found) continue;
    }

    spn_test_example_info_t info = {
      .path = entry->file_path,
      .name = entry->file_name,
      .main = {
        .c = sp_os_join_path(entry->file_path, sp_str_lit("main.c")),
        .cpp = sp_os_join_path(entry->file_path, sp_str_lit("main.cpp")),
      }
    };

    app.pad.name = SP_MAX(info.name.len, app.pad.name);

    spn_test_queue_t* queue = SP_NULLPTR;
    sp_dyn_array_for(app.queues, queue_idx) {
      if (sp_str_equal(app.queues[queue_idx].name, info.name)) {
        queue = &app.queues[queue_idx];
        break;
      }
    }
    if (!queue) {
      sp_dyn_array_push(app.queues, SP_ZERO_STRUCT(spn_test_queue_t));
      queue = sp_dyn_array_back(app.queues);
      queue->name = info.name;
      queue->index = 0;
    }

    sp_dyn_array(spn_test_build_kind_t) kinds = spn_test_collect_kinds_for_example(info.path, app.kind);

    sp_dyn_array(spn_test_build_mode_t) modes = SP_NULLPTR;
    sp_dyn_array_push(modes, SPN_TEST_BUILD_MODE_DEBUG);
    sp_dyn_array_push(modes, SPN_TEST_BUILD_MODE_RELEASE);

    sp_dyn_array_for(kinds, kind_index) {
      spn_test_build_kind_t kind = kinds[kind_index];
      sp_str_t kind_str = spn_test_build_kind_to_str(kind);
      app.pad.kind = SP_MAX(app.pad.kind, kind_str.len);

      sp_dyn_array_for(modes, mode_index) {
        spn_test_build_mode_t mode = modes[mode_index];
        sp_str_t mode_str = spn_test_build_mode_to_str(mode);
        app.pad.kind = SP_MAX(app.pad.kind, mode_str.len);

        bool should_skip = false;
        if (app.kind != SPN_TEST_BUILD_KIND_ALL && app.kind != kind) should_skip = true;
        if (app.mode != SPN_TEST_BUILD_MODE_ALL && app.mode != mode) should_skip = true;

        if (should_skip) {
          app.num_skipped++;
          continue;
        }

        u32 test_index = sp_dyn_array_size(app.tests);
        sp_dyn_array_push(app.tests, SP_ZERO_STRUCT(spn_test_descriptor_t));
        spn_test_descriptor_t* test = sp_dyn_array_back(app.tests);
        test->name = info.name;

        sp_dyn_array_push(queue->tests, test_index);
        test->kind = kind;
        test->mode = mode;
        test->status = app.cli.dry_run ? SPN_TEST_STATUS_DRY_RUN : SPN_TEST_STATUS_PENDING;
        test->tui_status = test->status;
        test->project_file = spn_project_file_for_kind(info.path, kind);
        test->language = sp_os_does_path_exist(info.main.c) ? SPN_TEST_LANGUAGE_C : SPN_TEST_LANGUAGE_CPP;
        test->main = test->language == SPN_TEST_LANGUAGE_C ? info.main.c : info.main.cpp;

        test->output.dir.relative = sp_format(
          "build/examples/{}/{}/{}",
          SP_FMT_STR(test->name),
          SP_FMT_STR(mode_str),
          SP_FMT_STR(kind_str)
        );
        test->output.dir.absolute = sp_os_join_path(app.paths.repo, test->output.dir.relative);
        test->output.executable = sp_os_join_path(test->output.dir.absolute, sp_str_lit("main"));
        test->output.logs = sp_os_join_path(test->output.dir.absolute, sp_str_lit("logs"));

        sp_mutex_init(&test->mutex, SP_MUTEX_PLAIN);

        test->commands.build = spn_make_command(
          sp_format(
            "{} --no-interactive -f {} --matrix {} build --force",
            SP_FMT_STR(app.paths.spn),
            SP_FMT_STR(test->project_file),
            SP_FMT_STR(mode_str)
          ),
          sp_os_join_path(test->output.logs, SP_LIT("build.log"))
        );

        bool needs_copy = kind == SPN_TEST_BUILD_KIND_SHARED;
        if (needs_copy) {
          test->commands.copy = spn_make_command(
            sp_format(
              "{} --no-interactive -f {} --matrix {} copy {}",
              SP_FMT_STR(app.paths.spn),
              SP_FMT_STR(test->project_file),
              SP_FMT_STR(mode_str),
              SP_FMT_STR(test->output.dir.relative)
            ),
            sp_os_join_path(test->output.logs, SP_LIT("copy.log"))
          );
        }

        test->commands.print = spn_make_command(
          sp_format(
            "{} --no-interactive -f {} --matrix {} print --compiler gcc",
            SP_FMT_STR(app.paths.spn),
            SP_FMT_STR(test->project_file),
            SP_FMT_STR(mode_str)
          ),
          sp_str_lit("")
        );

        test->commands.compile = spn_make_command(
          sp_format(
            "bear --append -- {} {} -g $({}) -Wl,-rpath,$ORIGIN -o {} -lm",
            SP_FMT_CSTR(test->language == SPN_TEST_LANGUAGE_C ? "gcc" : "g++"),
            SP_FMT_STR(test->main),
            SP_FMT_STR(test->commands.print.raw),
            SP_FMT_STR(test->output.executable)
          ),
          sp_os_join_path(test->output.logs, SP_LIT("compile.log"))
        );

      }
    }
  }

  if (!sp_dyn_array_size(app.tests) && !app.cli.dry_run) {
    sp_log(sp_format("{:fg yellow} no tests to run", SP_FMT_CSTR("!")));
    SP_EXIT_SUCCESS();
  }

  qsort(app.tests, sp_dyn_array_size(app.tests), sizeof(spn_test_descriptor_t), spn_test_qsort_kernel);

  if (app.cli.dry_run) {
    spn_print_summary();
    SP_EXIT_SUCCESS();
  }

  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;
    sp_mutex_init(&test->mutex, SP_MUTEX_PLAIN);
  }

  if (!app.cli.no_interactive) {
    sp_dyn_array_for(app.tests, index) {
      spn_test_descriptor_t* test = app.tests + index;
      spn_test_status_t status = test->status;

      sp_str_t row = spn_test_tui_format_row((spn_test_summary_row_t) {
        .status = spn_test_status_to_color(status, sp_str_pad(spn_test_status_to_str(status), app.pad.status)),
        .name = sp_str_pad(test->name, app.pad.name),
        .kind = sp_str_pad(spn_test_build_kind_to_str(test->kind), app.pad.kind),
        .mode = sp_str_pad(spn_test_build_mode_to_str(test->mode), app.pad.mode),
      });
      sp_log(row);
    }
  }

  if (app.cli.jobs > sp_dyn_array_size(app.queues)) {
    app.cli.jobs = sp_dyn_array_size(app.queues);
  }

  u32 next_queue_index = 0;

  while (true) {
    u32 num_running = spn_test_count_running_queues();

    while (num_running < app.cli.jobs) {
      bool found_work = false;
      u32 queues_checked = 0;

      while (queues_checked < sp_dyn_array_size(app.queues)) {
        spn_test_queue_t* queue = &app.queues[next_queue_index];

        spn_test_queue_advance(queue);

        if (queue->index < sp_dyn_array_size(queue->tests)) {
          u32 test_idx = queue->tests[queue->index];
          spn_test_descriptor_t* test = &app.tests[test_idx];

          sp_mutex_lock(&test->mutex);
          spn_test_status_t status = test->status;
          sp_mutex_unlock(&test->mutex);

          if (status == SPN_TEST_STATUS_PENDING) {
            sp_mutex_lock(&test->mutex);
            test->status = SPN_TEST_STATUS_BUILDING_DEPS;
            sp_mutex_unlock(&test->mutex);
            sp_thread_init(&test->thread, spn_test_build_async, test);

            found_work = true;
            next_queue_index = (next_queue_index + 1) % sp_dyn_array_size(app.queues);
            break;
          }
        }

        next_queue_index = (next_queue_index + 1) % sp_dyn_array_size(app.queues);
        queues_checked++;
      }

      if (!found_work) break;
      num_running = spn_test_count_running_queues();
    }

    spn_test_tui_update();
    if (spn_test_are_queues_done()) break;

    sp_os_sleep_ms(5);
  }

  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;
    sp_thread_join(&test->thread);
    sp_mutex_destroy(&test->mutex);
  }

  spn_print_summary();
  spn_log_failures();

  bool has_failure = false;
  sp_dyn_array_for(app.tests, index) {
    spn_test_descriptor_t* test = app.tests + index;
    if (test->status == SPN_TEST_STATUS_FAILED_DEPS || test->status == SPN_TEST_STATUS_FAILED_COMPILE) {
      has_failure = true;
      break;
    }
  }

  return has_failure ? 1 : 0;
}
