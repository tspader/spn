#ifndef SPN_ERROR_JSON_H
#define SPN_ERROR_JSON_H

#include "sp.h"
#include "error/types.h"

void spn_codegen_write_err(sp_io_writer_t* out, const spn_err_t* err);
void spn_codegen_write_semver(sp_io_writer_t* out, const spn_semver_t* version);
void spn_codegen_write_pkg_name(sp_io_writer_t* out, const spn_pkg_name_t* id);
void spn_codegen_write_requested_dep(sp_io_writer_t* out, const spn_requested_dep_t* request);
void spn_codegen_write_triple(sp_io_writer_t* out, const spn_triple_t* triple);
void spn_codegen_write_sanitizer_set(sp_io_writer_t* out, const spn_sanitizer_set_t* set);
void spn_codegen_write_option_violation(sp_io_writer_t* out, const spn_option_violation_t* violation);

#endif
