#define SP_IMPLEMENTATION
#include "sp.h"
#include "sqlite3.h"


s32 main(s32 num_args, c8** args) {
  sp_init((sp_config_t) {
    .allocator = sp_allocator_default()
  });

  // Open an in-memory database
  sqlite3* db;
  s32 rc = sqlite3_open(":memory:", &db);
  if (rc != SQLITE_OK) {
    SP_FATAL("Cannot open database: {}", SP_FMT_CSTR(sqlite3_errmsg(db)));
  }

  // Create a simple table
  const c8* create_sql = "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)";
  c8* err_msg = NULL;
  if (sqlite3_exec(db, create_sql, NULL, NULL, &err_msg) != SQLITE_OK) {
    SP_FATAL("Failed to create table: {}", SP_FMT_CSTR(err_msg));
  }

  // Insert a row
  const c8* insert_sql = "INSERT INTO users (name, age) VALUES ('Alice', 30)";
  if (sqlite3_exec(db, insert_sql, NULL, NULL, &err_msg) != SQLITE_OK) {
    SP_FATAL("Failed to insert data: {}", SP_FMT_CSTR(err_msg));
  }

  // Read the row back
  const c8* select_sql = "SELECT id, name, age FROM users";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL) != SQLITE_OK) {
    SP_FATAL("Failed to prepare statement: {}", SP_FMT_CSTR(sqlite3_errmsg(db)));
  }

  // Fetch and print the row
  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    s32 id = sqlite3_column_int(stmt, 0);
    const c8* name = (const c8*)sqlite3_column_text(stmt, 1);
    s32 age = sqlite3_column_int(stmt, 2);

    SP_LOG("Retrieved user: id={}, name='{}', age={}", SP_FMT_S32(id), SP_FMT_CSTR(name), SP_FMT_S32(age));
  }

  // Clean up
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return 0;
}

