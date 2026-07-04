#include "sqlite3.h"
#include <stdio.h>

int main(void) {
  sqlite3* db;
  char* err = 0;

  if (sqlite3_open(":memory:", &db) != SQLITE_OK) {
    fprintf(stderr, "open failed: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  if (sqlite3_exec(db,
        "CREATE TABLE t(a INTEGER, b TEXT);"
        "INSERT INTO t VALUES (1,'x'),(2,'y'),(3,'z');",
        0, 0, &err) != SQLITE_OK) {
    fprintf(stderr, "exec failed: %s\n", err);
    return 1;
  }

  sqlite3_stmt* st;
  if (sqlite3_prepare_v2(db, "SELECT count(*), sum(a) FROM t", -1, &st, 0) != SQLITE_OK) {
    fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  if (sqlite3_step(st) != SQLITE_ROW) {
    fprintf(stderr, "no row\n");
    return 1;
  }
  int count = sqlite3_column_int(st, 0);
  int sum   = sqlite3_column_int(st, 1);
  sqlite3_finalize(st);

  int fts_rc = sqlite3_exec(db,
      "CREATE VIRTUAL TABLE ft USING fts5(body);"
      "INSERT INTO ft VALUES ('hello world');",
      0, 0, &err);

  printf("sqlite %s: count=%d sum=%d fts5=%s\n",
         sqlite3_libversion(), count, sum,
         fts_rc == SQLITE_OK ? "ok" : sqlite3_errmsg(db));

  sqlite3_close(db);

  if (count != 3 || sum != 6 || fts_rc != SQLITE_OK) {
    return 2;
  }
  return 0;
}
