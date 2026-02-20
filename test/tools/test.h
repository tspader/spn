#ifndef SPN_TEST_H
#define SPN_TEST_H
#include "sp.h"

#define ut (*utest_fixture)
#define ur (*utest_result)

typedef struct {
  sp_str_t root;
} tmpfs_t;

void     tmpfs_init(tmpfs_t* fs);
sp_str_t tmpfs_get(tmpfs_t* fs, sp_str_t name);
void     tmpfs_create(tmpfs_t* fs, sp_str_t path, sp_str_t content);
sp_str_t tmpfs_touch(tmpfs_t* fs, sp_str_t path);
void     tmpfs_deinit(tmpfs_t* fs);
#endif
