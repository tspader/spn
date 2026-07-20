#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/atomic_file.h"
#include "spn.h"
#include "codegen.h"

typedef struct {
  spn_t* spn;
  sp_mem_t mem;
} codegen_io_t;

static void codegen_log(void* user, sp_str_t message) {
  codegen_io_t* io = (codegen_io_t*)user;
  spn_log(io->spn, sp_str_to_cstr(io->mem, message));
}

SPN_EXPORT
spn_err_t codegen(spn_t* spn, spn_node_ctx_t* ctx) {
  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());
  codegen_io_t io = { .spn = spn, .mem = mem };

  sp_str_t err = codegen_run(mem, (codegen_paths_t) {
    .schema = sp_str_lit("/source/source/codegen/schema"),
    .out = sp_str_lit("/source/source/codegen/gen"),
    .templates = sp_str_lit("/source/tools/templates"),
  }, codegen_log, &io);

  if (!sp_str_empty(err)) {
    spn_log(spn, sp_str_to_cstr(mem, err));
    return SPN_ERROR;
  }
  return SPN_OK;
}
