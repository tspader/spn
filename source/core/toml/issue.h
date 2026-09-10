#ifndef SPN_TOML_ISSUE_H
#define SPN_TOML_ISSUE_H

#include "codegen/types.h"
#include "spn/errors.h"

sp_da(spn_err_issue_t) spn_codegen_issues_to_err(sp_mem_t mem, sp_da(spn_codegen_issue_t) issues);

#endif

