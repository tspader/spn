#ifndef SPN_REFLECT_H
#define SPN_REFLECT_H

#include "reflect/types.h"
#include "spn/core.h"

spn_err_t spn_reflect_json_read(sp_str_t json, const spn_reflect_type_t* type, void* value, sp_mem_t mem);

#endif
