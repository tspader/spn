#ifndef SPN_OBJ_H
#define SPN_OBJ_H

#include "sp.h"
#include "sp/coff.h"
#include "sp/sp_elf.h"
#include "err.h"

typedef enum {
  SPN_OBJ_COFF,
  SPN_OBJ_ELF,
  SPN_OBJ_MACHO,
} spn_obj_kind_t;

typedef struct {
  spn_obj_kind_t kind;
  union {
    struct { sp_coff_t* coff; sp_coff_section_t* section; } coff;
    struct { sp_elf_t* elf; } elf;
  };
} spn_obj_builder_t;

spn_obj_kind_t spn_obj_get_native_format();
void           spn_obj_init(spn_obj_builder_t* obj, spn_obj_kind_t kind);
void           spn_obj_add_symbol(spn_obj_builder_t* obj, sp_str_t name, const void* data, u64 size);
spn_err_t      spn_obj_write(spn_obj_builder_t* obj, sp_str_t path);

#endif
