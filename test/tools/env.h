#ifndef SPN_TEST_ENV_H
#define SPN_TEST_ENV_H

#include "sp.h"
#include "sp/sp_test.h"
#include "fixture.h"

#define SPN_TEST_COMMAND_MAX_ARGS 9
#define SPN_TEST_COMMAND_MAX_ENV 4

typedef struct {
  sp_mem_t mem;
  sp_str_t root;
  sp_str_t events;
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
void      write_file(sp_str_t path, sp_str_t content);

sp_err_t prepare_test(sp_test_t* t, fixture_t* fixture, const c8* project, const c8* const* copy);
sp_ps_output_t run_spn_command(sp_test_t* t, fixture_t* fixture, const c8* output_mode, const c8* const* args, const c8* const* env);

#endif
