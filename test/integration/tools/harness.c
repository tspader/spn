#include "harness.h"
#include "error/error.h"
#include "triple/triple.h"
#include "yyjson.h"

#define expect_path(t, fixture, path) expect_exists(t, fixture, path, true, __FILE__, __LINE__)
#define expect_no_path(t, fixture, path) expect_exists(t, fixture, path, false, __FILE__, __LINE__)
#define expect_static_elf(t, fixture, path) expect_no_interp(t, fixture, path, __FILE__, __LINE__)

static SP_THREAD_LOCAL sp_mem_heap_t* harness_heap;

static sp_mem_t harness_mem() {
  if (!harness_heap) {
    harness_heap = sp_mem_heap_new();
  }
  return sp_mem_heap_as_allocator(harness_heap);
}

static sp_str_t layout_path(const c8* triple, const c8* profile, sp_str_t rest) {
  sp_mem_t mem = harness_mem();
  sp_str_t path = sp_str_lit("build");
  if (triple) {
    path = sp_fs_join_path(mem, path, sp_str_view(triple));
  }
  path = sp_fs_join_path(mem, path, sp_str_view(profile));
  return sp_fs_join_path(mem, path, rest);
}

static sp_str_t layout_sub(const c8* dir, const c8* rest) {
  return sp_fs_join_path(harness_mem(), sp_str_view(dir), sp_str_view(rest));
}

static const c8* shared_lib_file(const c8* name) {
  sp_mem_t mem = harness_mem();
  return sp_str_to_cstr(mem, spn_triple_lib_file_name(mem, test_host(), sp_str_view(name), SP_OS_LIB_SHARED));
}

sp_str_t shared_lib(const c8* name) {
  sp_mem_t mem = harness_mem();
  return store_file(sp_str_to_cstr(mem, sp_fs_join_path(mem, sp_str_lit("lib"), sp_str_view(shared_lib_file(name)))));
}

sp_str_t profile_static_lib(const c8* profile, const c8* name) {
  sp_mem_t mem = harness_mem();
  return sp_fmt(mem,
    "build/{}/store/lib/{}",
    sp_fmt_cstr(profile),
    sp_fmt_str(spn_triple_lib_file_name(mem, test_host(), sp_cstr_as_str(name), SP_OS_LIB_STATIC))
  ).value;
}

sp_str_t static_lib(const c8* name) {
  return profile_static_lib("debug", name);
}

sp_str_t staged_lib(const c8* name) {
  return exe(shared_lib_file(name));
}

sp_str_t test_lib(const c8* name) {
  return test_exe(shared_lib_file(name));
}

static sp_str_t exe_file_name(const c8* name, const c8* triple) {
  spn_triple_t target = test_host();
  if (triple) {
    spn_triple_t partial = sp_zero;
    sp_assert(spn_triple_parse(sp_str_view(triple), &partial) == SPN_OK);
    target = spn_triple_merge(target, partial);
  }
  if (sp_str_find_c8(sp_fs_get_name(sp_str_view(name)), '.') >= 0) {
    return sp_str_view(name);
  }
  return spn_triple_exe_file_name(harness_mem(), target, sp_str_view(name));
}

static sp_str_t profile_exe(const c8* profile, const c8* name) {
  return layout_path(SP_NULLPTR, profile, exe_file_name(name, SP_NULLPTR));
}

static const c8* store_rest(const c8* rest, const c8* triple) {
  if (sp_str_starts_with(sp_str_view(rest), sp_str_lit("bin/"))) {
    return sp_str_to_cstr(harness_mem(), exe_file_name(rest, triple));
  }
  return rest;
}

sp_str_t profile_store_file(const c8* profile, const c8* rest) {
  return layout_path(SP_NULLPTR, profile, layout_sub("store", store_rest(rest, SP_NULLPTR)));
}

sp_str_t exe(const c8* name) {
  return profile_exe("debug", name);
}

sp_str_t test_exe(const c8* name) {
  sp_str_t file = exe_file_name(name, SP_NULLPTR);
  return layout_path(SP_NULLPTR, "debug", layout_sub("test", sp_str_to_cstr(harness_mem(), file)));
}

sp_str_t example_exe(const c8* name) {
  sp_str_t file = exe_file_name(name, SP_NULLPTR);
  return layout_path(SP_NULLPTR, "debug", layout_sub("example", sp_str_to_cstr(harness_mem(), file)));
}

sp_str_t target_exe(const c8* name, const c8* triple) {
  return layout_path(triple, "debug", exe_file_name(name, triple));
}

sp_str_t store_file(const c8* rest) {
  return profile_store_file("debug", rest);
}

sp_str_t work_file(const c8* rest) {
  return layout_path(SP_NULLPTR, "debug", layout_sub(".spn", rest));
}

sp_str_t target_store_file(const c8* rest, const c8* triple) {
  return layout_path(triple, "debug", layout_sub("store", store_rest(rest, triple)));
}

static sp_str_t display_path(fixture_t* fixture, sp_str_t path) {
  if (!fixture) {
    return path;
  }
  sp_str_t relative = sp_str_strip_left(path, fixture->root);
  if (relative.len == path.len) {
    return path;
  }
  return sp_str_concat(harness_mem(), sp_str_lit("$test"), relative);
}

sp_err_t expect_exists(sp_test_t* t, fixture_t* fixture, sp_str_t path, bool expected, const c8* file, u32 line) {
  bool exists = sp_fs_exists(path);
  if (exists == expected) return SP_OK;

  if (fixture) {
    sp_test_kv(t, "root", fixture->root);
  }
  sp_test_kv(t, "path", display_path(fixture, path));
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str(expected ? "path exists" : "path does not exist"),
    .actual = sp_cstr_as_str(exists ? "it exists" : "it does not"),
  });
  return SP_ERR;
}

static sp_err_t expect_no_interp(sp_test_t* t, fixture_t* fixture, sp_str_t path, const c8* file, u32 line) {
  sp_io_file_reader_t reader = sp_zero;
  sp_must_ok(t, sp_io_file_reader_from_path(&reader, path));
  sp_io_seeking_reader_t elf = sp_zero;
  sp_io_seeking_reader_from_file_reader(&elf, &reader);
  sp_str_t interp = sp_zero;
  spn_err_t err = spn_elf_interp(fixture->mem, &elf, &interp);
  sp_io_file_reader_close(&reader);
  if (!err && sp_str_empty(interp)) {
    return SP_OK;
  }

  sp_test_kv(t, "root", fixture->root);
  sp_test_kv(t, "path", display_path(fixture, path));
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str("a static elf64 with no program interpreter"),
    .actual = err ? sp_cstr_as_str("not an elf64 image") : interp,
  });
  return SP_ERR;
}

static bool event_matches(yyjson_val* line, const c8* event, const c8* key, const c8* value) {
  const c8* name = yyjson_get_str(yyjson_obj_get(line, "event"));
  if (!name || !sp_cstr_equal(name, event)) return false;
  if (!key) return true;

  yyjson_val* field = yyjson_obj_get(line, key);
  if (!field) {
    field = yyjson_obj_get(yyjson_obj_get(line, "data"), key);
  }

  if (yyjson_is_int(field)) {
    sp_str_t num = sp_fmt(sp_mem_get_scratch(), "{}", sp_fmt_int(yyjson_get_sint(field))).value;
    return sp_str_equal_cstr(num, value);
  }

  const c8* str = yyjson_get_str(field);
  return str && sp_cstr_equal(str, value);
}

static u32 count_events(fixture_t* fixture, spn_event_kind_t kind, const c8* key, const c8* value) {
  const c8* event = spn_event_name(kind);
  sp_mem_t mem = fixture->mem;

  u32 count = 0;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, fixture->events, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(lines[it])) continue;

    yyjson_doc* doc = yyjson_read(lines[it].data, lines[it].len, 0);
    if (!doc) continue;

    if (event_matches(yyjson_doc_get_root(doc), event, key, value)) {
      count++;
    }
    yyjson_doc_free(doc);
  }
  return count;
}

static sp_err_t expect_event(sp_test_t* t, fixture_t* fixture, spn_event_kind_t kind, const c8* key, const c8* value, bool expected, const c8* file, u32 line) {
  const c8* event = spn_event_name(kind);
  sp_mem_t mem = fixture->mem;

  bool found = false;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, fixture->events, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(lines[it])) continue;

    yyjson_doc* doc = yyjson_read(lines[it].data, lines[it].len, 0);
    if (!doc) continue;

    found = event_matches(yyjson_doc_get_root(doc), event, key, value);
    yyjson_doc_free(doc);
    if (found) break;
  }

  if (found == expected) return SP_OK;

  sp_test_kv(t, "event", sp_str_view(event));
  if (key) {
    sp_test_kv(t, "field", sp_fmt(mem, "{} = {}",
      sp_fmt_cstr(key),
      sp_fmt_cstr(value)).value);
  }
  sp_test_kv(t, "events", fixture->events);
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str(expected ? "event logged" : "event not logged"),
    .actual = sp_cstr_as_str(found ? "it was logged" : "it was not"),
  });
  return SP_ERR;
}

static sp_err_t expect_result(sp_test_t* t, fixture_t* fixture, spn_err_t err, const c8* file, u32 line) {
  sp_mem_t mem = fixture->mem;

  sp_str_t actual = sp_zero;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, fixture->events, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(lines[it])) continue;

    yyjson_doc* doc = yyjson_read(lines[it].data, lines[it].len, 0);
    if (!doc) continue;

    yyjson_val* root = yyjson_doc_get_root(doc);
    const c8* name = yyjson_get_str(yyjson_obj_get(root, "event"));
    if (name && sp_cstr_equal(name, "result")) {
      const c8* code = yyjson_get_str(yyjson_obj_get(yyjson_obj_get(root, "data"), "err"));
      actual = code ? sp_test_format(t, "{}", sp_fmt_cstr(code)) : (sp_str_t) sp_zero;
    }
    yyjson_doc_free(doc);
  }

  if (!sp_str_empty(actual) && sp_str_equal(spn_err_to_str(err), actual)) return SP_OK;

  sp_test_kv(t, "events", fixture->events);
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = spn_err_to_str(err),
    .actual = sp_str_empty(actual) ? sp_str_lit("no result event") : actual,
  });
  return SP_ERR;
}

static sp_err_t expect_cc_arg(sp_test_t* t, fixture_t* fixture, const action_t* action, const c8* file, u32 line) {
  sp_mem_t mem = fixture->mem;
  bool expected = action->kind == ACTION_VERIFY_CC_ARG;

  sp_str_t path = fixture_path(fixture, sp_str_lit("compile_commands.json"));
  sp_str_t content = test_read_file(mem, path);

  sp_str_t list = sp_str_lit("");
  sp_carr_for(action->verify_cc_arg, it) {
    if (!action->verify_cc_arg[it]) {
      break;
    }
    list = sp_fmt(mem, "{} {.quote}", sp_fmt_str(list), sp_fmt_str(sp_str_view(action->verify_cc_arg[it]))).value;
  }

  yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
  yyjson_val* root = doc ? yyjson_doc_get_root(doc) : SP_NULLPTR;

  u32 num = (u32)yyjson_arr_size(root);
  bool ok = num || !expected;
  sp_str_t offender = sp_zero;

  sp_for(it, num) {
    yyjson_val* entry = yyjson_arr_get(root, it);
    yyjson_val* arguments = yyjson_obj_get(entry, "arguments");

    bool found = false;
    u32 num_args = (u32)yyjson_arr_size(arguments);
    sp_for(ai, num_args) {
      const c8* arg = yyjson_get_str(yyjson_arr_get(arguments, ai));
      if (!arg) {
        continue;
      }
      sp_carr_for(action->verify_cc_arg, alt) {
        if (!action->verify_cc_arg[alt]) {
          break;
        }
        if (sp_cstr_equal(arg, action->verify_cc_arg[alt])) {
          found = true;
        }
      }
    }

    if (found != expected) {
      ok = false;
      const c8* source = yyjson_get_str(yyjson_obj_get(entry, "file"));
      offender = source ? sp_str_view(source) : sp_str_lit("(unknown)");
    }
  }

  if (doc) {
    yyjson_doc_free(doc);
  }
  if (ok) {
    return SP_OK;
  }

  sp_test_kv(t, "args", list);
  sp_test_kv(t, "path", path);
  if (offender.len) {
    sp_test_kv(t, "entry", offender);
  }
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str(expected ? "compile argument present in every entry" : "compile argument absent from every entry"),
    .actual = sp_cstr_as_str(num ? (expected ? "an entry is missing it" : "an entry has it") : "no compile entries"),
  });
  return SP_ERR;
}

static sp_err_t expect_command_cc(sp_test_t* t, fixture_t* fixture, command_cc_t expected) {
  sp_mem_t mem = fixture->mem;
  sp_str_t path = fixture_path(fixture, sp_str_lit("compile_commands.json"));
  sp_str_t content = test_read_file(mem, path);

  sp_str_t list = sp_str_lit("");
  sp_carr_for(expected.args, it) {
    if (!expected.args[it]) {
      break;
    }
    list = sp_fmt(mem, "{} {.quote}", sp_fmt_str(list), sp_fmt_str(sp_str_view(expected.args[it]))).value;
  }

  yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
  yyjson_val* root = doc ? yyjson_doc_get_root(doc) : SP_NULLPTR;

  u32 num = (u32)yyjson_arr_size(root);
  u32 matches = 0;
  sp_for(it, num) {
    yyjson_val* entry = yyjson_arr_get(root, it);
    yyjson_val* arguments = yyjson_obj_get(entry, "arguments");

    bool found = false;
    u32 num_args = (u32)yyjson_arr_size(arguments);
    sp_for(ai, num_args) {
      const c8* arg = yyjson_get_str(yyjson_arr_get(arguments, ai));
      if (!arg) {
        continue;
      }
      sp_carr_for(expected.args, alt) {
        if (!expected.args[alt]) {
          break;
        }
        if (sp_str_contains(sp_str_view(arg), sp_str_view(expected.args[alt]))) {
          found = true;
        }
      }
    }
    if (found) {
      matches++;
    }
  }

  if (doc) {
    yyjson_doc_free(doc);
  }

  bool ok = expected.absent ? matches == 0 : matches > 0;
  if (ok) {
    return SP_OK;
  }

  sp_test_kv(t, "args", list);
  sp_test_kv(t, "path", path);
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(__FILE__),
    .line = (u32)__LINE__,
    .expected = sp_cstr_as_str(expected.absent ? "no compile entry has a matching argument" : "a compile entry has a matching argument"),
    .actual = sp_cstr_as_str(expected.absent ? "an entry has one" : (num ? "no entry has one" : "no compile entries")),
  });
  return SP_ERR;
}

static sp_ps_output_t run_fixture_bin(fixture_t* fixture, sp_str_t path) {
  return sp_ps_run(fixture->mem, (sp_ps_config_t) {
    .command = path,
    .cwd = fixture->root,
    .env = {
      .extra = {
        { sp_str_lit("ASAN_OPTIONS"), sp_str_lit("abort_on_error=0:exitcode=1") },
        { sp_str_lit("UBSAN_OPTIONS"), sp_str_lit("halt_on_error=1:abort_on_error=0:exitcode=1") },
      },
    },
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    },
  });
}

static sp_err_t run_command_bin(sp_test_t* t, fixture_t* fixture, command_bin_t bin) {
  sp_str_t staged = bin.path;
  if (sp_str_empty(staged)) {
    staged = bin.profile ? profile_exe(bin.profile, bin.name) : exe(bin.name);
  }
  sp_str_t path = fixture_path(fixture, staged);
  expect_path(t, fixture, path);

  if (bin.build_only) {
    return SP_OK;
  }

  sp_ps_output_t output = run_fixture_bin(fixture, path);

  sp_test_kv(t, "command", path);
  sp_test_kv(t, "output", output.out);
  sp_expect_eq(t, bin.rc, output.status.exit_code);

  sp_carr_for(bin.contains, it) {
    if (!bin.contains[it]) {
      break;
    }
    sp_test_kv(t, "needle", sp_str_view(bin.contains[it]));
    sp_expect(t, sp_str_contains(output.out, sp_str_view(bin.contains[it])));
  }
  return SP_OK;
}

static void expect_command_file(sp_test_t* t, fixture_t* fixture, command_file_t expected) {
  sp_str_t path = fixture_path(fixture, expected.file);
  sp_str_t content = test_read_file(fixture->mem, path);
  if (expected.content) {
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "expected", sp_str_view(expected.content));
    sp_test_kv(t, "content", content);
    sp_expect(t, sp_str_equal(content, sp_str_view(expected.content)));
  }
  sp_carr_for(expected.contains, it) {
    if (!expected.contains[it]) {
      break;
    }
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "needle", sp_str_view(expected.contains[it]));
    sp_test_kv(t, "content", content);
    sp_expect(t, sp_str_contains(content, sp_str_view(expected.contains[it])));
  }
  sp_carr_for(expected.excludes, it) {
    if (!expected.excludes[it]) {
      break;
    }
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "needle", sp_str_view(expected.excludes[it]));
    sp_test_kv(t, "content", content);
    sp_expect(t, !sp_str_contains(content, sp_str_view(expected.excludes[it])));
  }
  if (expected.json) {
    yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "content", content);
    sp_expect(t, doc != SP_NULLPTR);
    yyjson_doc_free(doc);
  }
}

static void expect_command_lock(sp_test_t* t, fixture_t* fixture, command_expect_t expected) {
  sp_str_t path = fixture_path(fixture, sp_str_lit("spn.lock"));
  expect_path(t, fixture, path);
  sp_str_t lock = test_read_file(fixture->mem, path);
  if (expected.lock) {
    sp_test_kv(t, "lock", lock);
    sp_expect(t, sp_str_contains(lock, sp_str_lit("[[dep]]")));
  }
  sp_carr_for(expected.packages, it) {
    if (!expected.packages[it]) {
      break;
    }
    sp_str_t needle = sp_fmt(fixture->mem, "name = \"{}\"", sp_fmt_cstr(expected.packages[it])).value;
    sp_test_kv(t, "needle", needle);
    sp_test_kv(t, "lock", lock);
    sp_expect(t, sp_str_contains(lock, needle));
  }
}

sp_err_t test_when(sp_test_t* t, test_when_t when) {
  sp_str_t blocked = test_when_blocked(when);
  if (blocked.len) {
    return sp_test_skip(t, "{}", sp_fmt_str(blocked));
  }
  return SP_OK;
}

sp_err_t run_command(sp_test_t* t, fixture_t* fixture, command_test_t test) {
  if (test.project) {
    sp_try(prepare_test(t, fixture, test.project, test.copy));
  }
  sp_ps_output_t output = (test.human ? run_spn : run_spn_json)(t, fixture, test.args, test.env);
  sp_expect_eq(t, test.expect.rc, output.status.exit_code);

  sp_str_t streams = sp_str_concat(fixture->mem, output.out, output.err);
  sp_carr_for(test.expect.contains, it) {
    if (!test.expect.contains[it]) {
      break;
    }
    sp_test_kv(t, "needle", sp_str_view(test.expect.contains[it]));
    sp_test_kv(t, "output", streams);
    sp_expect(t, sp_str_contains(streams, sp_str_view(test.expect.contains[it])));
  }

  sp_carr_for(test.expect.excludes, it) {
    if (!test.expect.excludes[it]) {
      break;
    }
    sp_test_kv(t, "needle", sp_str_view(test.expect.excludes[it]));
    sp_test_kv(t, "output", streams);
    sp_expect(t, !sp_str_contains(streams, sp_str_view(test.expect.excludes[it])));
  }

  sp_carr_for(test.expect.files, it) {
    command_file_t expected = test.expect.files[it];
    if (sp_str_empty(expected.file)) {
      break;
    }
    expect_command_file(t, fixture, expected);
  }

  sp_carr_for(test.expect.events, it) {
    command_event_t expected = test.expect.events[it];
    if (!expected.event) {
      break;
    }
    expect_event(t, fixture, expected.event, expected.key, expected.value, !expected.absent, __FILE__, __LINE__);
  }

  sp_carr_for(test.expect.cc, it) {
    if (!test.expect.cc[it].args[0]) {
      break;
    }
    expect_command_cc(t, fixture, test.expect.cc[it]);
  }

  sp_carr_for(test.expect.exists, it) {
    if (sp_str_empty(test.expect.exists[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, test.expect.exists[it]);
    expect_path(t, fixture, path);
  }

  sp_carr_for(test.expect.missing, it) {
    if (sp_str_empty(test.expect.missing[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, test.expect.missing[it]);
    expect_no_path(t, fixture, path);
  }

  if (test.expect.lock || test.expect.packages[0]) {
    expect_command_lock(t, fixture, test.expect);
  }

  if (test.expect.bin.name || !sp_str_empty(test.expect.bin.path)) {
    sp_try(run_command_bin(t, fixture, test.expect.bin));
  }
  return SP_OK;
}

sp_err_t run_command_test(sp_test_t* t, command_test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));
  sp_try(test_when(t, test.when));
  if (!test.project) {
    sp_try(prepare_test(t, &fixture, SP_NULLPTR, SP_NULLPTR));
  }
  return run_command(t, &fixture, test);
}

static sp_err_t apply_rebuild_change(sp_test_t* t, fixture_t* fixture, rebuild_change_t change) {
  sp_carr_for(change.remove_files, it) {
    if (sp_str_empty(change.remove_files[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, change.remove_files[it]);
    sp_fs_remove_file(path);
    expect_no_path(t, fixture, path);
  }

  sp_carr_for(change.moves, it) {
    if (sp_str_empty(change.moves[it].from)) {
      break;
    }
    sp_str_t from = fixture_path(fixture, change.moves[it].from);
    sp_str_t to = fixture_path(fixture, change.moves[it].to);
    sp_str_t content = test_read_file(fixture->mem, from);
    fixture_create(fixture, change.moves[it].to, content);
    sp_fs_remove_file(from);
    expect_no_path(t, fixture, from);
    expect_path(t, fixture, to);
  }

  sp_carr_for(change.writes, it) {
    if (sp_str_empty(change.writes[it].file)) {
      break;
    }
    fixture_create(fixture, change.writes[it].file, change.writes[it].content);
  }

  sp_carr_for(change.remove_dirs, it) {
    if (sp_str_empty(change.remove_dirs[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, change.remove_dirs[it]);
    sp_fs_remove_dir(path);
    expect_no_path(t, fixture, path);
  }
  return SP_OK;
}

sp_err_t run_rebuild_test(sp_test_t* t, rebuild_test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));

  sp_try(test_when(t, test.when));

  sp_try(prepare_test(t, &fixture, test.project, test.copy));
  sp_try(run_command(t, &fixture, test.first));

  sp_tm_epoch_t mtimes[SPN_TEST_REBUILD_MAX_WATCHES] = sp_zero;
  sp_carr_for(test.watches, it) {
    if (sp_str_empty(test.watches[it].file)) {
      break;
    }
    sp_str_t path = fixture_path(&fixture, test.watches[it].file);
    expect_path(t, &fixture, path);
    mtimes[it] = sp_fs_get_mod_time(path);
  }

  sp_carr_for(test.rebuilds, it) {
    if (!test.rebuilds[it].command.args[0]) {
      break;
    }
    sp_try(apply_rebuild_change(t, &fixture, test.rebuilds[it].change));
    sp_try(run_command(t, &fixture, test.rebuilds[it].command));
  }

  sp_carr_for(test.watches, it) {
    rebuild_watch_t watch = test.watches[it];
    if (sp_str_empty(watch.file)) {
      break;
    }
    sp_str_t path = fixture_path(&fixture, watch.file);
    sp_tm_epoch_t now = sp_fs_get_mod_time(path);
    bool unchanged = mtimes[it].s == now.s && mtimes[it].ns == now.ns;
    if (watch.mtime == REBUILD_MTIME_UNCHANGED) {
      sp_expect(t, unchanged);
    }
    if (watch.mtime == REBUILD_MTIME_CHANGED) {
      sp_expect(t, !unchanged);
    }
  }
  return SP_OK;
}

sp_err_t run_actions(sp_test_t* t, fixture_t* fixture, const action_t* actions) {
  sp_mem_t mem = fixture->mem;

  sp_for(it, SPN_TEST_MAX_ACTIONS) {
    action_t action = actions[it];
    if (action.kind == ACTION_NONE) {
      break;
    }

    sp_test_kv_clear(t, SP_NULLPTR);

    switch (action.kind) {
      case ACTION_NONE: {
        break;
      }
      case ACTION_CREATE_FILE: {
        fixture_create(fixture, action.create.file, action.create.content);
        break;
      }
      case ACTION_RUN_BIN:
      case ACTION_RUN_TEST: {
        sp_str_t staged_bin = action.kind == ACTION_RUN_TEST ? test_exe(action.bin.name) : exe(action.bin.name);
        sp_str_t bin = fixture_path(fixture, staged_bin);
        expect_path(t, fixture, bin);

        sp_ps_output_t output = run_fixture_bin(fixture, bin);

        sp_test_kv(t, "command", bin);
        sp_test_kv(t, "output", output.out);
        sp_expect_eq(t, action.bin.rc, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_EXISTS: {
        sp_str_t path = fixture_path(fixture, action.exists);
        expect_path(t, fixture, path);
        break;
      }
      case ACTION_VERIFY_NOT_EXISTS: {
        sp_str_t path = fixture_path(fixture, action.exists);
        expect_no_path(t, fixture, path);
        break;
      }
      case ACTION_VERIFY_NO_INTERP: {
        sp_str_t path = fixture_path(fixture, action.verify_no_interp);
        expect_static_elf(t, fixture, path);
        break;
      }
      case ACTION_VERIFY_DIR_COUNT: {
        sp_str_t path = fixture_path(fixture, sp_str_view(action.verify_dir_count.dir));
        sp_da(sp_fs_entry_t) entries = sp_zero;
        sp_fs_collect(mem, path, &entries);
        u32 dirs = 0;
        sp_da_for(entries, et) {
          if (entries[et].kind == SP_FS_KIND_DIR) {
            dirs++;
          }
        }
        sp_test_kv(t, "path", path);
        sp_expect_eq(t, action.verify_dir_count.count, dirs);
        break;
      }
      case ACTION_VERIFY_INCLUDE: {
        sp_str_t path = fixture_path(
          fixture,
          sp_fs_join_path(mem, store_file("include"), action.verify_include.file)
        );
        expect_path(t, fixture, path);
        break;
      }
      case ACTION_VERIFY_FILE_CONTAINS: {
        sp_str_t path = fixture_path(fixture, action.verify_file_contains.file);
        sp_str_t content = test_read_file(mem, path);
        sp_test_kv(t, "path", path);
        sp_test_kv(t, "needle", action.verify_file_contains.needle);
        sp_test_kv(t, "content", content);
        sp_expect(t, sp_str_contains(content, action.verify_file_contains.needle));
        break;
      }
      case ACTION_VERIFY_FILE_NOT_CONTAINS: {
        sp_str_t path = fixture_path(fixture, action.verify_file_not_contains.file);
        sp_str_t content = test_read_file(mem, path);
        sp_test_kv(t, "path", path);
        sp_test_kv(t, "needle", action.verify_file_not_contains.needle);
        sp_test_kv(t, "content", content);
        sp_expect(t, !sp_str_contains(content, action.verify_file_not_contains.needle));
        break;
      }
      case ACTION_REMOVE_DIR: {
        sp_str_t path = fixture_path(fixture, sp_str_view(action.rm.dir));
        sp_fs_remove_dir(path);
        expect_no_path(t, fixture, path);
        break;
      }
      case ACTION_RUN_CLI: {
        const c8* args[SPN_TEST_COMMAND_MAX_ARGS] = { action.cli.cmd };
        sp_carr_for(action.cli.args, it) {
          if (!action.cli.args[it]) {
            break;
          }
          args[it + 1] = action.cli.args[it];
        }
        sp_ps_output_t output = run_spn_json(t, fixture, args, action.cli.env);
        sp_expect_eq(t, action.cli.rc, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_CC_ARG:
      case ACTION_VERIFY_NO_CC_ARG: {
        expect_cc_arg(t, fixture, &action, __FILE__, __LINE__);
        break;
      }
      case ACTION_VERIFY_EVENT:
      case ACTION_VERIFY_NO_EVENT: {
        expect_event(t, fixture, action.verify_event.event, action.verify_event.key, action.verify_event.value, action.kind == ACTION_VERIFY_EVENT, __FILE__, __LINE__);
        break;
      }
      case ACTION_VERIFY_RESULT: {
        expect_result(t, fixture, action.verify_result.err, __FILE__, __LINE__);
        break;
      }
      case ACTION_VERIFY_EVENT_COUNT: {
        u32 count = count_events(fixture, action.verify_event_count.event, action.verify_event_count.key, action.verify_event_count.value);
        sp_test_kv(t, "event", sp_str_view(spn_event_name(action.verify_event_count.event)));
        sp_expect_eq(t, action.verify_event_count.count, count);
        break;
      }
      case ACTION_VERIFY_LOCKED: {
        sp_str_t path = fixture_path(fixture, sp_str_lit("spn.lock"));
        expect_path(t, fixture, path);

        sp_str_t lock = test_read_file(mem, path);
        sp_test_kv(t, "lock", lock);
        sp_expect(t, sp_str_contains(lock, sp_str_lit("[[dep]]")));
        break;
      }
      case ACTION_VERIFY_PKG_LOCKED: {
        sp_str_t path = fixture_path(fixture, sp_str_lit("spn.lock"));
        expect_path(t, fixture, path);

        sp_str_t lock = test_read_file(mem, path);
        sp_str_t needle = sp_fmt(mem, "name = \"{}\"", sp_fmt_cstr(action.verify_locked.name)).value;
        sp_test_kv(t, "needle", needle);
        sp_test_kv(t, "lock", lock);
        sp_expect(t, sp_str_contains(lock, needle));
        break;
      }
    }
  }
  return SP_OK;
}

sp_err_t run_test(sp_test_t* t, test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));

  sp_try(test_when(t, test.when));

  if (!test_when_runs(&test.when)) {
    u32 kept = 0;
    sp_carr_for(test.actions, it) {
      action_t action = test.actions[it];
      if (action.kind == ACTION_RUN_BIN || action.kind == ACTION_RUN_TEST) {
        continue;
      }
      test.actions[kept++] = action;
    }
    while (kept < SPN_TEST_MAX_ACTIONS) {
      test.actions[kept++] = (action_t) { .kind = ACTION_NONE };
    }
  }

  sp_try(prepare_test(t, &fixture, test.project, test.copy));
  return run_actions(t, &fixture, test.actions);
}

static bool opt_build_present(const opt_build_t* build) {
  return build->present || build->profile || build->manifest || build->target ||
    build->alternate || build->expect.rc || build->expect.bin.name;
}

static sp_err_t opt_set_manifest(sp_test_t* t, fixture_t* fixture, const c8* manifest) {
  sp_str_t from = fixture_path(fixture, sp_cstr_as_str(manifest));
  sp_str_t to = fixture_path(fixture, sp_str_lit("spn.toml"));
  sp_str_t content = test_read_file(fixture->mem, from);
  fixture_create(fixture, sp_str_lit("spn.toml"), content);
  sp_fs_remove_file(from);
  sp_must(t, !sp_fs_exists(from));
  sp_must(t, sp_fs_exists(to));
  return SP_OK;
}

sp_err_t run_opt_test(sp_test_t* t, opt_test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));

  sp_try(test_when(t, test.when));

  sp_try(prepare_test(t, &fixture, test.project, test.copy));

  const test_toolchain_t* toolchain = test_toolchain();
  u32 ran = 0;
  sp_str_t blocked = sp_zero;
  sp_carr_for(test.builds, it) {
    const opt_build_t* build = &test.builds[it];
    if (!opt_build_present(build)) {
      break;
    }

    const c8* target = build->target;
    if (build->alternate) {
      target = test_target_alternate();
      if (!target) {
        blocked = sp_fmt(fixture.mem, "{} has no cross target", sp_fmt_cstr(toolchain->name)).value;
        continue;
      }
    }

    test_when_t when = build->when;
    if (!when.target) {
      when.target = target;
    }
    blocked = test_when_blocked(when);
    if (blocked.len) {
      continue;
    }

    if (build->manifest) {
      sp_try(opt_set_manifest(t, &fixture, build->manifest));
    }

    command_test_t command = {
      .args = { "build" },
      .expect = build->expect,
    };
    u32 arg = 1;
    if (build->profile) {
      command.args[arg++] = "-p";
      command.args[arg++] = build->profile;
      command.expect.bin.profile = build->profile;
    }
    if (target) {
      command.args[arg++] = "--target";
      command.args[arg++] = target;
    }
    if ((command.expect.bin.name || command.expect.bin.path.len) && !test_when_runs(&when)) {
      command.expect.bin.build_only = true;
    }
    sp_try(run_command(t, &fixture, command));
    ran++;
  }

  if (!ran) {
    return sp_test_skip(t, "{}", sp_fmt_str(blocked));
  }
  return SP_OK;
}
