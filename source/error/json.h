#ifndef SPN_ERROR_JSON_H
#define SPN_ERROR_JSON_H

#include "sp.h"
#include "spn/core.h"

void spn_codegen_write_err(sp_io_writer_t* out, const spn_err_t* err);
void spn_codegen_write_semver(sp_io_writer_t* out, const spn_semver_t* version);
bool spn_codegen_semver_present(const spn_semver_t* version);
void spn_codegen_write_triple(sp_io_writer_t* out, const spn_triple_t* triple);
void spn_codegen_write_sanitizer_set(sp_io_writer_t* out, const spn_sanitizer_set_t* set);

#endif
