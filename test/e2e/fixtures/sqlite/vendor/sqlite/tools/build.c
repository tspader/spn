#include "spn.h"
#include "mksqlite3h_tcl.h"
#include "mksqlite3c_tcl.h"
#include "mkfts5c_tcl.h"

#include <unistd.h>

int lemon_main(int argc, char** argv);
int mkkeywordhash_main(int argc, char** argv);
int jimsh_main(int argc, char* const* argv);

extern const char* spn_jimsh_stdin_path;
extern const char* spn_jimsh_stdout_path;

#define LOG(msg)  spn_log(spn, msg)
#define FAIL(msg)  do { spn_log(spn, msg); return 1; } while (0)

typedef struct {
  const c8* in;
  const c8* out;
  const c8* args [8];
} jimsh_t;

static int run_jimsh(jimsh_t config) {
  spn_jimsh_stdin_path = config.in;
  spn_jimsh_stdout_path = config.out;

  int n = 0;
  for (int it = 0; it < 8; it++) {
    if (!config.args[it]) break;
    n++;
  }

  int rc = jimsh_main(n, (c8* const*)config.args);
  spn_jimsh_stdin_path = 0;
  spn_jimsh_stdout_path = 0;
  return rc;
}

SPN_EXPORT
s32 generate_code(spn_t* spn, spn_node_ctx_t* ctx) {
  (void)ctx;
  if (chdir("/work") != 0) FAIL("chdir /work");

  struct {
    jimsh_t opcodes_h;
    jimsh_t opcodes_c;
    jimsh_t pragma_h;
    jimsh_t ctime_c;
    jimsh_t sqlite3_h;
    jimsh_t fts5_c;
    jimsh_t sqlite3_c;
  } jims = {
    .opcodes_h = {
      .in = "/work/.opcodeh.in",
      .out = "/work/opcodes.h",
      .args = { "jimsh", "/source/tool/mkopcodeh.tcl" },
    },
    .opcodes_c = {
      .out = "/work/opcodes.c",
      .args = { "jimsh", "/source/tool/mkopcodec.tcl", "opcodes.h" },
    },
    .pragma_h = {
      .args = { "jimsh", "/source/tool/mkpragmatab.tcl", "pragma.h" },
    },
    .ctime_c = {
      .args = { "jimsh", "/source/tool/mkctimec.tcl", "ctime.c" },
    },
    .sqlite3_h = {
      .args = { "jimsh", "/work/mksqlite3h.tcl", "/source", "-o", "sqlite3.h" },
    },
    .fts5_c = {
      .args = { "jimsh", "/work/mkfts5c.tcl", "/source/ext/fts5" },
    },
    .sqlite3_c = {
      .args = { "jimsh", "/work/mksqlite3c.tcl", "--linemacros=0" },
    },
  };

  spn_fs_create_dir("/work/tsrc");
  spn_io_write("/work/mksqlite3h.tcl", SPN_MKSQLITE3H_TCL);
  spn_io_write("/work/mksqlite3c.tcl", SPN_MKSQLITE3C_TCL);
  spn_io_write("/work/mkfts5c.tcl", SPN_MKFTS5C_TCL);

  spn_fs_copy("/source/src/parse.y", "/work");
  spn_fs_copy("/source/tool/lempar.c", "/work");
  spn_fs_copy("/source/ext/fts5/fts5parse.y", "/work");

  LOG("mkkeywordhash -> keywordhash.h");
  if (mkkeywordhash_main(2, (char*[]){ "mkkeywordhash", "keywordhash.h", 0 })) FAIL("mkkeywordhash");

  LOG("lemon parse.y");
  if (lemon_main(2, (char*[]){ "lemon", "parse.y", 0 })) FAIL("lemon parse.y");

  LOG("lemon fts5parse.y");
  if (lemon_main(2, (char*[]){ "lemon", "fts5parse.y", 0 })) FAIL("lemon fts5parse.y");

  LOG("mkopcodeh.tcl -> opcodes.h");
  spn_fs_cat("/work/.opcodeh.in", "/work/parse.h", "/source/src/vdbe.c");
  if (run_jimsh(jims.opcodes_h)) FAIL("mkopcodeh");

  LOG("mkopcodec.tcl -> opcodes.c");
  if (run_jimsh(jims.opcodes_c)) FAIL("mkopcodec");

  LOG("mkpragmatab.tcl -> pragma.h");
  if (run_jimsh(jims.pragma_h)) FAIL("mkpragmatab");

  LOG("mkctimec.tcl -> ctime.c");
  if (run_jimsh(jims.ctime_c)) FAIL("mkctimec");

  LOG("mksqlite3h.tcl -> sqlite3.h");
  if (run_jimsh(jims.sqlite3_h)) FAIL("mksqlite3h");

  LOG("mkfts5c.tcl -> fts5.c");
  if (run_jimsh(jims.fts5_c)) FAIL("mkfts5c");

  spn_fs_copy_glob("/source/src/*.c", "/work/tsrc");
  spn_fs_copy_glob("/source/src/*.h", "/work/tsrc");

  static const c8* ext_files [] = {
    "ext/fts3/fts3.c", "ext/fts3/fts3.h", "ext/fts3/fts3Int.h", "ext/fts3/fts3_aux.c",
    "ext/fts3/fts3_expr.c", "ext/fts3/fts3_hash.c", "ext/fts3/fts3_hash.h", "ext/fts3/fts3_icu.c",
    "ext/fts3/fts3_porter.c", "ext/fts3/fts3_snippet.c", "ext/fts3/fts3_tokenizer.h",
    "ext/fts3/fts3_tokenizer.c", "ext/fts3/fts3_tokenizer1.c", "ext/fts3/fts3_tokenize_vtab.c",
    "ext/fts3/fts3_unicode.c", "ext/fts3/fts3_unicode2.c", "ext/fts3/fts3_write.c",
    "ext/icu/sqliteicu.h", "ext/icu/icu.c",
    "ext/rtree/rtree.h", "ext/rtree/rtree.c", "ext/rtree/geopoly.c",
    "ext/session/sqlite3session.c", "ext/session/sqlite3session.h",
    "ext/rbu/sqlite3rbu.h", "ext/rbu/sqlite3rbu.c",
    "ext/misc/stmt.c",
    "ext/fts5/fts5.h",
    0
  };
  for (const c8** it = ext_files; *it; it++) {
    spn_fs_copy(spn_fmt("/source/{}", *it), "/work/tsrc");
  }

  static const c8* gen_files [] = {
    "parse.c", "parse.h", "opcodes.c", "opcodes.h", "keywordhash.h",
    "pragma.h", "ctime.c", "sqlite3.h", "fts5.c", 0
  };
  for (const c8** it = gen_files; *it; it++) {
    spn_fs_copy(spn_fmt("/work/{}", *it), "/work/tsrc");
  }

  LOG("mksqlite3c.tcl -> sqlite3.c");
  if (run_jimsh(jims.sqlite3_c)) FAIL("mksqlite3c");

  spn_fs_copy("/work/sqlite3.c", "/source/sqlite3.c");

  LOG("done: sqlite3.c + sqlite3.h generated");
  return 0;
}
