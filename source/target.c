#include "target.h"
#include "ctx.h"
#include "intern.h"
#include "external/cc.h"

sp_str_t spn_visibility_to_str(spn_visibility_t kind) {
  switch (kind) {
    case SPN_VISIBILITY_PUBLIC: {
      return spn_intern_cstr("public");
    }
    case SPN_VISIBILITY_TEST: {
      return spn_intern_cstr("test");
    }
    case SPN_VISIBILITY_BUILD: {
      return spn_intern_cstr("build");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_visibility_t spn_visibility_from_str(sp_str_t str) {
  if (spn_intern_is_equal_cstr(str, "public")) {
    return SPN_VISIBILITY_PUBLIC;
  }
  if (spn_intern_is_equal_cstr(str, "test")) {
    return SPN_VISIBILITY_TEST;
  }
  if (spn_intern_is_equal_cstr(str, "build")) {
    return SPN_VISIBILITY_BUILD;
  }

  SP_UNREACHABLE_RETURN(SPN_VISIBILITY_PUBLIC);
}

spn_linkage_t spn_lib_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "shared")) {
    return SPN_LIB_KIND_SHARED;
  }
  if (sp_str_equal_cstr(str, "static")) {
    return SPN_LIB_KIND_STATIC;
  }
  if (sp_str_equal_cstr(str, "source")) {
    return SPN_LIB_KIND_SOURCE;
  }

  SP_FATAL("Unknown lib kind {:fg brightyellow}; options are [shared, static, source]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_LIB_KIND_SHARED);
}

spn_linkage_t spn_pkg_linkage_from_str(sp_str_t str) {
  return spn_lib_kind_from_str(str);
}

sp_str_t spn_pkg_linkage_to_str(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: {
      return sp_str_lit("shared");
    }
    case SPN_LIB_KIND_STATIC: {
      return sp_str_lit("static");
    }
    case SPN_LIB_KIND_SOURCE: {
      return sp_str_lit("source");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: {
      return SP_OS_LIB_SHARED;
    }
    case SPN_LIB_KIND_STATIC: {
      return SP_OS_LIB_STATIC;
    }
    case SPN_LIB_KIND_SOURCE: {
      return 0;
    }
  }

  SP_UNREACHABLE_RETURN(0);
}

void spn_linkage_set_add(spn_linkage_set_t* set, spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SOURCE: {
      set->source = true;
      break;
    }
    case SPN_LIB_KIND_SHARED: {
      set->shared = true;
      break;
    }
    case SPN_LIB_KIND_STATIC: {
      set->static_lib = true;
      break;
    }
  }
}

bool spn_linkage_set_has(spn_linkage_set_t set, spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SOURCE: {
      return set.source;
    }
    case SPN_LIB_KIND_SHARED: {
      return set.shared;
    }
    case SPN_LIB_KIND_STATIC: {
      return set.static_lib;
    }
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_linkage_t spn_linkage_set_default(spn_linkage_set_t set) {
  if (set.source) {
    return SPN_LIB_KIND_SOURCE;
  }

  if (set.static_lib) {
    return SPN_LIB_KIND_STATIC;
  }

  if (set.shared) {
    return SPN_LIB_KIND_SHARED;
  }

  SP_UNREACHABLE_RETURN(SPN_LIB_KIND_SHARED);
}

spn_target_kind_t spn_pkg_linkage_to_target_kind(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: return SPN_TARGET_SHARED_LIB;
    case SPN_LIB_KIND_STATIC: return SPN_TARGET_STATIC_LIB;
    case SPN_LIB_KIND_SOURCE: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(SPN_TARGET_EXE);
}

spn_linkage_t spn_target_kind_to_pkg_linkage(spn_target_kind_t kind) {
  switch (kind) {
    case SPN_TARGET_SHARED_LIB: return SPN_LIB_KIND_SHARED;
    case SPN_TARGET_STATIC_LIB: return SPN_LIB_KIND_STATIC;
    case SPN_TARGET_NONE:
    case SPN_TARGET_EXE:
    case SPN_TARGET_JIT:
    case SPN_TARGET_OBJECT: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(SPN_LIB_KIND_SHARED);
}

void spn_target_add_source(spn_target_t* target, const c8* source) {
  sp_require(target);
  spn_target_add_source_ex(target, sp_str_view(source));
}

void spn_target_add_source_ex(spn_target_t* target, sp_str_t source) {
  sp_require(target);

  spn_ctx_push_target_source_event(target, source);

  source = spn_intern(source);
  sp_da_push(target->source, source);
}

void spn_target_add_include(spn_target_t* target, const c8* include) {
  sp_require(target);
  spn_target_add_include_ex(target, sp_str_view(include));
}

void spn_target_add_include_ex(spn_target_t* target, sp_str_t include) {
  sp_require(target);
  sp_da_push(target->include, spn_intern(include));
}

void spn_target_add_define(spn_target_t* target, const c8* define) {
  sp_require(target);
  spn_target_add_define_ex(target, sp_str_view(define));
}

void spn_target_add_define_ex(spn_target_t* target, sp_str_t define) {
  sp_require(target);
  sp_da_push(target->define, spn_intern(define));
}

void spn_target_set_visibility(spn_target_t* target, spn_visibility_t visibility) {
  sp_require(target);
  target->visibility = visibility;
}

void spn_target_embed_file(spn_target_t* target, const c8* file) {
  spn_target_embed_file_ex_s(target, sp_str_view(file), SP_EMBED_DEFAULT_SYMBOL_S, SP_EMBED_DEFAULT_DATA_T_S, SP_EMBED_DEFAULT_SIZE_T_S);
}

void spn_target_embed_file_ex(
  spn_target_t* target,
  const c8* file,
  const c8* symbol,
  const c8* data_type,
  const c8* size_type
) {
  spn_target_embed_file_ex_s(target, sp_str_view(file), sp_str_view(symbol), sp_str_view(data_type), sp_str_view(size_type));
}

void spn_target_embed_file_ex_s(
  spn_target_t* target,
  sp_str_t file,
  sp_str_t symbol,
  sp_str_t data_type,
  sp_str_t size_type
) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_FILE,
    .symbol = spn_intern(symbol),
    .types = {
      .data = spn_intern(data_type),
      .size = spn_intern(size_type),
    },
    .file = {
      .path = spn_intern(file),
    }
  }));
}

void spn_target_embed_mem_ex_s(
  spn_target_t* target,
  sp_str_t symbol,
  const u8* buffer,
  u64 size,
  sp_str_t data_type,
  sp_str_t size_type
) {
  sp_da_push(target->embed, ((spn_embed_t) {
    .kind = SPN_EMBED_MEM,
    .symbol = spn_intern(symbol),
    .types = {
      .data = spn_intern(data_type),
      .size = spn_intern(size_type),
    },
    .memory = {
      .buffer = buffer,
      .size = size
    }
  }));
}

void spn_target_embed_mem(spn_target_t* target, const c8* symbol, const u8* buffer, u64 buffer_size) {
  spn_target_embed_mem_ex(target, symbol, buffer, buffer_size, SP_EMBED_DEFAULT_DATA_T, SP_EMBED_DEFAULT_SIZE_T);
}

void spn_target_embed_mem_ex(
  spn_target_t* target,
  const c8* symbol,
  const u8* buffer,
  u64 size,
  const c8* data_type,
  const c8* size_type
) {
  spn_target_embed_mem_ex_s(target, sp_str_view(symbol), buffer, size, sp_str_view(data_type), sp_str_view(size_type));
}

void spn_target_embed_dir(spn_target_t* target, const c8* dir) {
  spn_target_embed_dir_ex(target, dir, SP_EMBED_DEFAULT_DATA_T, SP_EMBED_DEFAULT_SIZE_T);
}

void spn_target_embed_dir_ex(spn_target_t* target, const c8* dir, const c8* data_type, const c8* size_type) {
  spn_embed_t embed = {
    .types = {
      .data = spn_intern_cstr(data_type),
      .size = spn_intern_cstr(size_type),
    }
  };

  sp_str_t root = sp_str_view(dir);

  sp_da(sp_os_dir_ent_t) entries = sp_fs_collect_recursive(root);
  sp_da_for(entries, it) {
    sp_os_dir_ent_t* entry = &entries[it];
    if (sp_fs_is_regular_file(entry->file_path)) {
      spn_target_embed_file_ex_s(
        target,
        entry->file_path,
        spn_cc_symbol_from_embedded_file(
          sp_str_suffix(
            entry->file_path,
            entry->file_path.len - root.len - 1
          )
        ),
        embed.types.data,
        embed.types.size
      );
    }
  }
}
