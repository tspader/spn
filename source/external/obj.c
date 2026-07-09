#include "obj.h"

spn_obj_kind_t spn_obj_get_native_format() {
  switch (sp_os_get_kind()) {
    case SP_OS_WIN32: return SPN_OBJ_COFF;
    case SP_OS_LINUX: return SPN_OBJ_ELF;
    case SP_OS_MACOS: return SPN_OBJ_MACHO;
  }
  return SPN_OBJ_ELF;
}

void spn_obj_init(spn_obj_builder_t* obj, sp_mem_t mem, spn_obj_kind_t kind, spn_arch_t arch) {
  obj->kind = kind;
  switch (kind) {
    case SPN_OBJ_COFF: {
      obj->coff.coff = sp_coff_new(mem);
      obj->coff.section = sp_coff_add_section(obj->coff.coff, sp_str_lit(".rdata"),
        SP_COFF_SCN_CNT_INITIALIZED_DATA |
        SP_COFF_SCN_ALIGN_8BYTES |
        SP_COFF_SCN_MEM_READ);
    } break;
    case SPN_OBJ_ELF: {
      obj->elf.elf = sp_elf_new(mem);
      obj->elf.rodata = sp_elf_add_section(obj->elf.elf, (sp_elf_section_t){
        .name = sp_str_lit(".rodata"),
        .type = SHT_PROGBITS,
        .flags = SHF_ALLOC | SHF_WRITE,
        .align = 8,
      });
    } break;
    case SPN_OBJ_MACHO: {
      u32 cpu = arch == SPN_ARCH_X64 ? SP_MACHO_CPU_X86_64 : SP_MACHO_CPU_ARM64;
      u32 subtype = arch == SPN_ARCH_X64 ? SP_MACHO_SUBTYPE_X86_64 : SP_MACHO_SUBTYPE_ARM64;
      obj->macho.macho = sp_macho_new(mem, cpu, subtype);
      obj->macho.section = sp_macho_add_section(obj->macho.macho, sp_str_lit("__DATA"), sp_str_lit("__const"), 3);
    } break;
    default: SP_ASSERT(false); break;
  }
}

void spn_obj_add_symbol(spn_obj_builder_t* obj, sp_str_t name, const void* data, u64 size) {
  switch (obj->kind) {
    case SPN_OBJ_COFF: {
      u64 offset = obj->coff.section->writer.storage.len;
      sp_io_write(&obj->coff.section->writer.base, data, size, SP_NULLPTR);
      sp_coff_add_symbol(obj->coff.coff, name, (u32)offset, 1, SP_COFF_SYM_CLASS_EXTERNAL);
    } break;
    case SPN_OBJ_ELF: {
      u64 offset = sp_elf_append_aligned(obj->elf.elf, obj->elf.rodata, data, size, 8);
      sp_elf_add_symbol(obj->elf.elf, (sp_elf_symbol_t){
        .name = name,
        .section = obj->elf.rodata,
        .value = offset,
        .size = size,
        .bind = STB_GLOBAL,
        .type = STT_OBJECT,
      });
    } break;
    case SPN_OBJ_MACHO: {
      sp_io_dyn_mem_writer_t* writer = &obj->macho.section->writer;
      u64 offset = (writer->storage.len + 7) & ~7ull;
      if (offset > writer->storage.len) {
        sp_io_pad(&writer->base, offset - writer->storage.len, SP_NULLPTR);
      }
      sp_io_write(&writer->base, data, size, SP_NULLPTR);

      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_macho_add_symbol(obj->macho.macho, sp_fmt(scratch.mem, "_{}", sp_fmt_str(name)).value, 1, offset);
      sp_mem_end_scratch(scratch);
    } break;
    default: SP_ASSERT(false); break;
  }
}

spn_err_t spn_obj_write(spn_obj_builder_t* obj, sp_str_t path) {
  switch (obj->kind) {
    case SPN_OBJ_COFF:  spn_try_as(sp_coff_write_to_file(obj->coff.coff, path), SPN_ERROR); break;
    case SPN_OBJ_ELF:   spn_try_as(sp_elf_write_to_file(obj->elf.elf, path), SPN_ERROR); break;
    case SPN_OBJ_MACHO: spn_try_as(sp_macho_write_to_file(obj->macho.macho, path), SPN_ERROR); break;
    default: SP_ASSERT(false); break;
  }
  return SPN_OK;
}
