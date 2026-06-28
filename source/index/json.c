#include "index/json.h"

#include "external/mz.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "sp.h"
#include "sp/str.h"

#define mz_assign(t, ptr, value) *((t*)(ptr)) = (value)
#define mz_deref(t, value) *((t*)(value))
#define spn_cstr const c8*

static mz_err_t on_parse_str(mz_ctx_t* ctx, void* parent, mz_key_t key, void* ptr, const void* value) {
  sp_str_t str = sp_str_view(mz_deref(spn_cstr, value));
  mz_assign(sp_str_t, ptr, str);
  return MZ_OK;
}

static mz_err_t on_parse_semver(mz_ctx_t* ctx, void* parent, mz_key_t key, void* ptr, const void* value) {
  sp_str_t version = sp_str_view(mz_deref(spn_cstr, value));
  if (sp_str_empty(version)) {
    return MZ_ERR_RANGE;
  }

  spn_semver_t semver = spn_semver_from_str(version);
  if (!sp_str_equal(version, spn_semver_to_str(semver))) {
    return MZ_ERR_RANGE;
  }

  mz_assign(spn_semver_t, ptr, semver);

  return MZ_OK;
}

static const c8* dep_kind_strings[] = {
  [SPN_INDEX_DEP_NORMAL] = "normal",
  [SPN_INDEX_DEP_BUILD]  = "build",
  [SPN_INDEX_DEP_TEST]   = "test",
};

static const c8* spn_index_dep_kind_to_cstr(spn_index_dep_kind_t kind) {
  sp_assert(kind <= SPN_INDEX_DEP_TEST);
  return dep_kind_strings[kind];
}

static mz_err_t on_parse_dep_kind(mz_ctx_t* ctx, void* parent, mz_key_t key, void* ptr, const void* value) {
  sp_str_t str = sp_str_view(mz_deref(spn_cstr, value));
  sp_for(it, sp_carr_len(dep_kind_strings)) {
    if (sp_str_equal_cstr(str, dep_kind_strings[it])) {
      mz_assign(spn_index_dep_kind_t, ptr, (spn_index_dep_kind_t)it);
      return MZ_OK;
    }
  }
  return MZ_ERR_RANGE;
}

static mz_err_t on_alloc_dep(mz_ctx_t* ctx, void* parent, mz_key_t key, u32 size, void** ptr) {
  sp_da(spn_index_dep_t)* deps = parent;
  sp_da_push(*deps, SP_ZERO_STRUCT(spn_index_dep_t));
  *ptr = &(*deps)[sp_da_size(*deps) - 1];
  return MZ_OK;
}

static spn_err_t spn_index_parse_rel(mz_ctx_t* ctx, mz_schema_t* schema, spn_pkg_id_t id, sp_str_t json, spn_index_rel_t* release) {
  c8* source = sp_str_to_cstr(spn_mem_todo, json);

  mz_ctx_clear(ctx);
  spn_try_as(mz_parse_str_ex(schema, source, release, ctx), SPN_ERROR);

  if (!sp_str_equal(release->id.namespace, id.namespace)) {
    return SPN_ERROR;
  }
  if (!sp_str_equal(release->id.name, id.name)) {
    return SPN_ERROR;
  }

  return SPN_OK;
}

static s32 sort_release_by_version(const void* a, const void* b) {
  const spn_index_rel_t* lhs = (const spn_index_rel_t*)a;
  const spn_index_rel_t* rhs = (const spn_index_rel_t*)b;
  return spn_semver_cmp(lhs->version, rhs->version);
}

mz_schema_t* spn_index_build_schema(mz_ctx_t* ctx) {
  mz_builder_t b = mz_builder_begin();
  MZ_SCHEMA(&b, MZ_OBJECT_LOOSE) {
    MZ_BIND_PARSE(&b, spn_index_rel_t, id.namespace, "namespace", mz_schema_string(), on_parse_str);
    MZ_BIND_PARSE(&b, spn_index_rel_t, id.name, "name", mz_schema_string(), on_parse_str);
    MZ_BIND_PARSE(&b, spn_index_rel_t, version, "version", mz_schema_string(), on_parse_semver);
    MZ_BIND_EX(&b, spn_index_rel_t, checksum, "checksum", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
    MZ_BIND(&b, spn_index_rel_t, yanked, "yanked", mz_schema_bool());
    MZ_BIND_OBJECT_EX(&b, spn_index_rel_t, source, "source", MZ_OBJECT_LOOSE, MZ_OPTIONAL, MZ_DEFAULT_ALLOC) {
      MZ_BIND_EX(&b, spn_index_rel_source_t, url, "url", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
      MZ_BIND_EX(&b, spn_index_rel_source_t, rev, "rev", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
      MZ_BIND_EX(&b, spn_index_rel_source_t, dir, "dir", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
    }
    MZ_BIND_OBJECT_EX(&b, spn_index_rel_t, manifest, "manifest", MZ_OBJECT_LOOSE, MZ_OPTIONAL, MZ_DEFAULT_ALLOC) {
      MZ_BIND_EX(&b, spn_index_rel_source_t, url, "url", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
      MZ_BIND_EX(&b, spn_index_rel_source_t, rev, "rev", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
      MZ_BIND_EX(&b, spn_index_rel_source_t, dir, "dir", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
    }
    MZ_BIND_OBJECT_EX(&b, spn_index_rel_t, paths, "paths", MZ_OBJECT_LOOSE, MZ_OPTIONAL, MZ_DEFAULT_ALLOC) {
      MZ_BIND_EX(&b, spn_index_rel_paths_t, manifest, "manifest", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
      MZ_BIND_EX(&b, spn_index_rel_paths_t, script, "script", mz_schema_string(), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, on_parse_str);
    }
    MZ_BIND_ARRAY_EX(&b, spn_index_rel_t, deps, "deps", MZ_OPTIONAL, on_alloc_dep) {
      MZ_ENTRY_OBJECT(&b, MZ_OBJECT_LOOSE) {
        MZ_BIND_PARSE(&b, spn_index_dep_t, id.namespace, "namespace", mz_schema_string(), on_parse_str);
        MZ_BIND_PARSE(&b, spn_index_dep_t, id.name, "name", mz_schema_string(), on_parse_str);
        MZ_BIND_PARSE(&b, spn_index_dep_t, version, "version", mz_schema_string(), on_parse_str);
        MZ_BIND_PARSE(&b, spn_index_dep_t, kind, "kind", mz_schema_string(), on_parse_dep_kind);
      }
    }
  }

  return mz_builder_end(&b);
}

static void json_append_str(sp_str_builder_t* b, const c8* key, sp_str_t val) {
  sp_str_builder_append_cstr(b, "\"");
  sp_str_builder_append_cstr(b, key);
  sp_str_builder_append_cstr(b, "\": \"");
  sp_str_builder_append(b, val);
  sp_str_builder_append_cstr(b, "\"");
}

static void json_append_bool(sp_str_builder_t* b, const c8* key, bool val) {
  sp_str_builder_append_cstr(b, "\"");
  sp_str_builder_append_cstr(b, key);
  sp_str_builder_append_cstr(b, "\": ");
  sp_str_builder_append_cstr(b, val ? "true" : "false");
}

static void json_append_source(sp_str_builder_t* b, const c8* key, spn_index_rel_source_t* src) {
  if (sp_str_empty(src->url)) { return; }

  sp_str_builder_append_cstr(b, ", \"");
  sp_str_builder_append_cstr(b, key);
  sp_str_builder_append_cstr(b, "\": {");
  json_append_str(b, "url", src->url);
  if (!sp_str_empty(src->rev)) {
    sp_str_builder_append_cstr(b, ", ");
    json_append_str(b, "rev", src->rev);
  }
  if (!sp_str_empty(src->dir)) {
    sp_str_builder_append_cstr(b, ", ");
    json_append_str(b, "dir", src->dir);
  }
  sp_str_builder_append_cstr(b, "}");
}

sp_str_t spn_index_rel_to_json(spn_index_rel_t* rel) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();

  sp_str_builder_append_cstr(&b, "{");
  json_append_str(&b, "namespace", rel->id.namespace);
  sp_str_builder_append_cstr(&b, ", ");
  json_append_str(&b, "name", rel->id.name);
  sp_str_builder_append_cstr(&b, ", ");
  json_append_str(&b, "version", spn_semver_to_str(rel->version));
  sp_str_builder_append_cstr(&b, ", ");
  json_append_bool(&b, "yanked", rel->yanked);

  json_append_source(&b, "source", &rel->source);
  json_append_source(&b, "manifest", &rel->manifest);

  if (!sp_str_empty(rel->paths.manifest) || !sp_str_empty(rel->paths.script)) {
    sp_str_builder_append_cstr(&b, ", \"paths\": {");
    bool need_comma = false;
    if (!sp_str_empty(rel->paths.manifest)) {
      json_append_str(&b, "manifest", rel->paths.manifest);
      need_comma = true;
    }
    if (!sp_str_empty(rel->paths.script)) {
      if (need_comma) { sp_str_builder_append_cstr(&b, ", "); }
      json_append_str(&b, "script", rel->paths.script);
    }
    sp_str_builder_append_cstr(&b, "}");
  }

  if (!sp_da_empty(rel->deps)) {
    sp_str_builder_append_cstr(&b, ", \"deps\": [");
    sp_da_for(rel->deps, it) {
      if (it > 0) { sp_str_builder_append_cstr(&b, ", "); }
      sp_str_builder_append_cstr(&b, "{");
      json_append_str(&b, "namespace", rel->deps[it].id.namespace);
      sp_str_builder_append_cstr(&b, ", ");
      json_append_str(&b, "name", rel->deps[it].id.name);
      sp_str_builder_append_cstr(&b, ", ");
      json_append_str(&b, "version", rel->deps[it].version);
      sp_str_builder_append_cstr(&b, ", ");
      json_append_str(&b, "kind", sp_str_view(spn_index_dep_kind_to_cstr(rel->deps[it].kind)));
      sp_str_builder_append_cstr(&b, "}");
    }
    sp_str_builder_append_cstr(&b, "]");
  }

  sp_str_builder_append_cstr(&b, "}");
  return sp_str_builder_to_str(&b);
}

spn_err_t spn_index_parse_pkg(mz_ctx_t* ctx, mz_schema_t* schema, spn_pkg_id_t id, sp_str_t blob, spn_index_pkg_t* pkg) {
  pkg->id = id;
  sp_str_for_line(blob, it) {
    sp_str_t line = sp_str_trim(it.line);
    if (sp_str_empty(line)) {
      continue;
    }

    spn_index_rel_t release = SP_ZERO_INITIALIZE();
    spn_try(spn_index_parse_rel(ctx, schema, id, line, &release));

    sp_da_for(pkg->releases, n) {
      if (spn_semver_eq(pkg->releases[n].version, release.version)) {
        return SPN_ERROR;
      }
    }

    sp_da_push(pkg->releases, release);
  }

  if (sp_da_empty(pkg->releases)) {
    return SPN_ERROR;
  }

  sp_dyn_array_sort(pkg->releases, sort_release_by_version);
  return SPN_OK;
}
