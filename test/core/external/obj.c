#include "unit.h"

#include "external/obj.h"

typedef struct {
  const c8* name;
  spn_obj_kind_t kind;
  spn_arch_t arch;
  u32 machine;
} machine_t;

static const machine_t machine_tests [] = {
  { "elf_x86_64",    SPN_OBJ_ELF,   SPN_ARCH_X64,   EM_X86_64 },
  { "elf_aarch64",   SPN_OBJ_ELF,   SPN_ARCH_ARM64, EM_AARCH64 },
  { "coff_x86_64",   SPN_OBJ_COFF,  SPN_ARCH_X64,   SP_COFF_MACHINE_AMD64 },
  { "coff_aarch64",  SPN_OBJ_COFF,  SPN_ARCH_ARM64, SP_COFF_MACHINE_ARM64 },
  { "macho_x86_64",  SPN_OBJ_MACHO, SPN_ARCH_X64,   SP_MACHO_CPU_X86_64 },
  { "macho_aarch64", SPN_OBJ_MACHO, SPN_ARCH_ARM64, SP_MACHO_CPU_ARM64 },
};

static sp_err_t write_obj(spn_obj_builder_t* obj, sp_io_writer_t* out) {
  switch (obj->kind) {
    case SPN_OBJ_COFF:  return sp_coff_write(obj->coff.coff, out);
    case SPN_OBJ_ELF:   return sp_elf_write(obj->elf.elf, out);
    case SPN_OBJ_MACHO: return sp_macho_write(obj->macho.macho, out);
  }
  SP_UNREACHABLE_RETURN(SP_ERR);
}

static u32 read_machine(spn_obj_kind_t kind, sp_str_t bytes) {
  u16 half = 0;
  u32 word = 0;
  switch (kind) {
    case SPN_OBJ_COFF: {
      sp_mem_copy(&half, bytes.data, sizeof(half));
      return half;
    }
    case SPN_OBJ_ELF: {
      sp_mem_copy(&half, bytes.data + offsetof(Elf64_Ehdr, e_machine), sizeof(half));
      return half;
    }
    case SPN_OBJ_MACHO: {
      sp_mem_copy(&word, bytes.data + sizeof(u32), sizeof(word));
      return word;
    }
  }
  SP_UNREACHABLE_RETURN(0);
}

sp_test_each(obj, machine, machine_t, machine_tests) {
  sp_mem_t mem = sp_test_arena(t);

  spn_obj_builder_t obj = sp_zero;
  spn_obj_init(&obj, mem, it->kind, it->arch);
  u8 data [] = { 1, 2, 3, 4 };
  spn_obj_add_symbol(&obj, sp_str_lit("blob"), data, sizeof(data));

  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &writer);
  sp_must_eq(t, SP_OK, write_obj(&obj, &writer.base));

  sp_str_t bytes = sp_io_dyn_mem_writer_as_str(&writer);
  sp_must(t, bytes.len >= SP_COFF_FILE_HEADER_SIZE);
  sp_expect_eq(t, it->machine, read_machine(it->kind, bytes));
  return SP_OK;
}
