#include "spn.h"

// #define SP_IMPLEMENTATION
// #include "sp.h"
//
// #include "cJSON.h"
// #include "cJSON.c"

#include "sqlite3.h"

void configure(spn_build_ctx_t* b) {
  spn_log(b, "hello!");

  sqlite3 *db;
  sqlite3_open(":memory:", &db);

  sqlite3_exec(db, "CREATE TABLE t(x)", 0, 0, 0);
  sqlite3_exec(db, "INSERT INTO t VALUES('hello, sqlite!')", 0, 0, 0);

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, "SELECT x FROM t", -1, &stmt, 0);
  sqlite3_step(stmt);
  spn_log(b, (const c8*)sqlite3_column_text(stmt, 0));

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  // sp_str_t foo = sp_str_lit("hello from sp.h!");
  // spn_log(b, sp_str_to_cstr(foo));
  //
  // cJSON* json = cJSON_CreateObject();
  // cJSON_AddNumberToObject(json, "filmore", 69);
  // cJSON_AddStringToObject(json, "guitar", "jerry");
  //
  // spn_log(b, cJSON_Print(json));

}
