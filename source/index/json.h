#ifndef SPN_INDEX_JSON_H
#define SPN_INDEX_JSON_H

#include "error/types.h"
#include "index/types.h"

mz_schema_t* spn_index_build_schema(mz_ctx_t* ctx);
spn_err_t spn_index_parse_pkg(mz_ctx_t* ctx, mz_schema_t* schema, spn_pkg_id_t id, sp_str_t blob, spn_index_pkg_t* pkg);
sp_str_t spn_index_rel_to_json(spn_index_rel_t* rel);

#endif
