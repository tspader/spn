#include <stdio.h>
#include <sqlite3.h>

int main(void) {
  printf("SQLite version: %s\n", sqlite3_libversion());

  sqlite3* db;
  int rc = sqlite3_open(":memory:", &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  char* err = 0;
  rc = sqlite3_exec(db, "CREATE TABLE test(id INTEGER PRIMARY KEY, name TEXT)", 0, 0, &err);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err);
    sqlite3_free(err);
    sqlite3_close(db);
    return 1;
  }

  rc = sqlite3_exec(db, "INSERT INTO test(name) VALUES('hello')", 0, 0, &err);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err);
    sqlite3_free(err);
    sqlite3_close(db);
    return 1;
  }

  printf("Created table and inserted row successfully\n");
  sqlite3_close(db);
  return 0;
}
