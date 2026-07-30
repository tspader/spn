#ifndef SPN_TEST_HARNESS_H
#define SPN_TEST_HARNESS_H

#include "sp.h"
#include "spit.h"
#include "fixture.h"
#include "action.h"
#include "command.h"
#include "rebuild.h"

typedef struct {
  sp_mem_t mem;
  sp_str_t root;
  struct {
    sp_str_t root;
    sp_str_t spn;
    sp_str_t storage;
    sp_str_t toolchain;
    sp_str_t config;
    sp_str_t index;
    sp_str_t include;
    sp_str_t patches;
  } paths;
} fixture_t;

fixture_t fixture_new(sp_test_t* t);
sp_err_t  fixture_init(sp_test_t* t, fixture_t* fixture);
sp_str_t  fixture_path(fixture_t* fixture, sp_str_t relative);
void      fixture_create(fixture_t* fixture, sp_str_t relative, sp_str_t content);

sp_str_t shared_lib(const c8* name);
sp_str_t static_lib(const c8* name);
sp_str_t profile_static_lib(const c8* profile, const c8* name);
sp_str_t staged_lib(const c8* name);
sp_str_t test_lib(const c8* name);
sp_str_t exe(const c8* name);
sp_str_t test_exe(const c8* name);
sp_str_t target_exe(const c8* name, const c8* triple);
sp_str_t store_file(const c8* rest);
sp_str_t work_file(const c8* rest);
sp_str_t profile_store_file(const c8* profile, const c8* rest);
sp_str_t target_store_file(const c8* rest, const c8* triple);

sp_err_t expect_exists(sp_test_t* t, fixture_t* fixture, sp_str_t path, bool expected, const c8* file, u32 line);

sp_err_t prepare_test(sp_test_t* t, fixture_t* fixture, const c8* project, const c8* const* copy);
sp_err_t run_command(sp_test_t* t, fixture_t* fixture, command_test_t test);
sp_err_t run_command_test(sp_test_t* t, command_test_t test);
sp_err_t run_rebuild_test(sp_test_t* t, rebuild_test_t test);
sp_err_t run_actions(sp_test_t* t, fixture_t* fixture, const action_t* actions);
sp_err_t run_test(sp_test_t* t, test_t test);

#endif
