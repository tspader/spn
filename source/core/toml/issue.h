#ifndef SPN_TOML_ISSUE_H
#define SPN_TOML_ISSUE_H

#include "codegen/types.h"
#include "spn/errors.h"

void      spn_codegen_issue_write(sp_io_writer_t* w, const spn_codegen_issue_t* issue);
sp_str_t  spn_codegen_issues_message(sp_mem_t mem, sp_da(spn_codegen_issue_t) issues);
sp_str_t  spn_err_issues_message(sp_mem_t mem, sp_da(spn_err_issue_t) issues);
sp_da(spn_err_issue_t) spn_codegen_issues_to_err(sp_mem_t mem, sp_da(spn_codegen_issue_t) issues);

#endif

