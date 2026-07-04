#include "spn.h"
#include "mksqlite3h_tcl.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

int lemon_main(int argc, char** argv);
int mkkeywordhash_main(int argc, char** argv);
int jimsh_main(int argc, char* const* argv);

#define LOG(msg)  spn_log(spn, msg)
#define FAIL(msg)  do { spn_log(spn, msg); return 1; } while (0)

static int copy_file(const char* src, const char* dst) {
  FILE* in = fopen(src, "rb");
  if (!in) { fprintf(stderr, "[sqlite-codegen] read %s failed\n", src); return -1; }
  FILE* out = fopen(dst, "wb");
  if (!out) { fprintf(stderr, "[sqlite-codegen] write %s failed\n", dst); fclose(in); return -1; }
  char buf[1 << 16]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
  fclose(in); fclose(out);
  return 0;
}

static int copy_suffix(const char* srcdir, const char* suffix, const char* dstdir) {
  DIR* d = opendir(srcdir);
  if (!d) { fprintf(stderr, "[sqlite-codegen] opendir %s failed\n", srcdir); return -1; }
  size_t slen = strlen(suffix);
  struct dirent* e; int count = 0;
  while ((e = readdir(d))) {
    size_t nlen = strlen(e->d_name);
    if (nlen < slen || strcmp(e->d_name + nlen - slen, suffix) != 0) continue;
    char s[1024], t[1024];
    snprintf(s, sizeof s, "%s/%s", srcdir, e->d_name);
    snprintf(t, sizeof t, "%s/%s", dstdir, e->d_name);
    if (copy_file(s, t) == 0) count++;
  }
  closedir(d);
  return count;
}

static int copy_list(const char* srcroot, const char** rel, const char* dstdir) {
  for (const char** p = rel; *p; p++) {
    const char* base = strrchr(*p, '/'); base = base ? base + 1 : *p;
    char s[1024], t[1024];
    snprintf(s, sizeof s, "%s/%s", srcroot, *p);
    snprintf(t, sizeof t, "%s/%s", dstdir, base);
    if (copy_file(s, t) != 0) return -1;
  }
  return 0;
}

static int cat2(const char* a, const char* b, const char* out) {
  FILE* o = fopen(out, "wb"); if (!o) return -1;
  const char* ins[] = { a, b, 0 };
  for (int i = 0; ins[i]; i++) {
    FILE* in = fopen(ins[i], "rb"); if (!in) { fclose(o); return -1; }
    char buf[1 << 16]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, o);
    fclose(in);
  }
  fclose(o);
  return 0;
}

extern const char* spn_jimsh_stdin_path;
extern const char* spn_jimsh_stdout_path;

static int run_jimsh(const char* in_path, const char* out_path, char* const* argv, int argc) {
  spn_jimsh_stdin_path = in_path;
  spn_jimsh_stdout_path = out_path;
  int rc = jimsh_main(argc, argv);
  spn_jimsh_stdin_path = 0;
  spn_jimsh_stdout_path = 0;
  return rc;
}

typedef struct {
  const c8* in;
  const c8* out;
  const c8* args [8];
} jimsh_t;

static int run_jimsh2(jimsh_t config) {
  spn_jimsh_stdin_path = config.in;
  spn_jimsh_stdout_path = config.out;
  int n = 0;
  for (int it = 0; it < 8; it++) {
    if (!config.args[it]) break;
    n++;
  }
  return jimsh_main(n, (c8* const*)config.args);
}

SPN_EXPORT
s32 generate_code(spn_t* spn, spn_node_ctx_t* ctx) {
  (void)ctx;
  if (chdir("/work") != 0) FAIL("chdir /work");
  mkdir("/work/tsrc", 0777);

  spn_log(spn, "Building mksqlite3h.tcl");
  spn_write_file(spn, "mksqlite3h.tcl", SPN_MKSQLITE3H_TCL);

  spn_fs_copy("/source/src/parse.y", "/work/parse.y");
  spn_fs_copy("/source/tool/lempar.c", "/work/lempar.c");
  spn_fs_copy("/source/ext/fts5/fts5parse.y", "/work/fts5parse.y");

  LOG("mkkeywordhash -> keywordhash.h");
  if (mkkeywordhash_main(2, (char*[]){ "mkkeywordhash", "keywordhash.h", 0 })) FAIL("mkkeywordhash");

  LOG("lemon parse.y");
  if (lemon_main(2, (char*[]){ "lemon", "parse.y", 0 })) FAIL("lemon parse.y");

  LOG("lemon fts5parse.y");
  if (lemon_main(2, (char*[]){ "lemon", "fts5parse.y", 0 })) FAIL("lemon fts5parse.y");

  LOG("mkopcodeh.tcl -> opcodes.h");   /* reads stdin (parse.h + vdbe.c), writes stdout */
  if (cat2("parse.h", "/source/src/vdbe.c", "/work/.opcodeh.in")) FAIL("cat opcodeh input");

  // @spader Do it like this
  struct {
    jimsh_t opcodes_h;
  } jims = {
    .opcodes_h = {
      .in = "/work/.opcodeh.in",
      .out = "/work/opcodes.h",
      .args = {
        "jimsh", "/source/tool/mkopcodeh.tcl"
      }
    }
  };
  if (run_jimsh2(jims.opcodes_h)) FAIL("mkopcodeh");

  LOG("mkopcodec.tcl -> opcodes.c");   /* reads opcodes.h (argv), writes stdout */
  if (run_jimsh(0, "/work/opcodes.c",
                (char* const[]){ "jimsh", "/source/tool/mkopcodec.tcl", "opcodes.h", 0 }, 3)) FAIL("mkopcodec");

  LOG("mkpragmatab.tcl -> pragma.h");  /* writes destfile arg */
  if (run_jimsh(0, 0, (char* const[]){ "jimsh", "/source/tool/mkpragmatab.tcl", "pragma.h", 0 }, 3)) FAIL("mkpragmatab");

  LOG("mkctimec.tcl -> ctime.c");
  if (run_jimsh(0, 0, (char* const[]){ "jimsh", "/source/tool/mkctimec.tcl", "ctime.c", 0 }, 3)) FAIL("mkctimec");

  LOG("mksqlite3h.tcl -> sqlite3.h");  /* writes via -o */
  if (run_jimsh(0, 0, (char* const[]){ "jimsh", "/work/mksqlite3h.tcl", "/source", "-o", "sqlite3.h", 0 }, 5)) FAIL("mksqlite3h");

  LOG("mkfts5c.tcl -> fts5.c");        /* writes fts5.c in cwd */
  if (run_jimsh(0, 0, (char* const[]){ "jimsh", "/source/ext/fts5/tool/mkfts5c.tcl", 0 }, 2)) FAIL("mkfts5c");

  if (copy_suffix("/source/src", ".c", "/work/tsrc") < 0) FAIL("stage src/*.c");
  if (copy_suffix("/source/src", ".h", "/work/tsrc") < 0) FAIL("stage src/*.h");
  static const char* ext_files[] = {
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
  if (copy_list("/source", ext_files, "/work/tsrc")) FAIL("stage ext files");
  static const char* gen_files[] = {
    "parse.c", "parse.h", "opcodes.c", "opcodes.h", "keywordhash.h",
    "pragma.h", "ctime.c", "sqlite3.h", "fts5.c", 0
  };
  if (copy_list("/work", gen_files, "/work/tsrc")) FAIL("stage generated files");

  LOG("mksqlite3c.tcl -> sqlite3.c");  /* reads tsrc/, writes sqlite3.c in cwd */
  if (run_jimsh(0, 0, (char* const[]){ "jimsh", "/source/tool/mksqlite3c.tcl", "--linemacros=0", 0 }, 3)) FAIL("mksqlite3c");

  if (spn_copy(spn, SPN_DIR_WORK, "sqlite3.c", SPN_DIR_SOURCE, "sqlite3.c")) FAIL("copy sqlite3.c -> source");

  LOG("done: sqlite3.c + sqlite3.h generated");
  return 0;
}
