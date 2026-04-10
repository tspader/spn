// #include "app/app.h"
//
// #include "ctx/ctx.h"
// #include "event/event.h"
// #include "log/log.h"
// #include "unit/types.h"
// #include "pkg/id.h"
// #include "pkg/mutate.h"
// #include "pkg/pkg.h"
// #include "ctx/types.h"
// #include "session/session.h"
// #include "unit/package.h"
//
// #define SPN_API_LOG(ctx, fn_name, args_fmt, ...) \
//   spn_event_buffer_push_ex(spn.events, (ctx)->pkg, &(ctx)->logs, (spn_build_event_t) { \
//     .kind = SPN_EVENT_API_CALL, \
//     .api_call = { .fn = sp_str_lit(fn_name), .args = sp_format(args_fmt, ##__VA_ARGS__) } \
//   })
//
// spn_node_t* spn_add_node(spn_build_ctx_t* c, const c8* tag) {
//   SPN_API_LOG(c, "spn_add_node", "{}", SP_FMT_CSTR(tag));
//   spn_pkg_unit_t* unit = (spn_pkg_unit_t*)c;
//   u32 index = sp_da_size(unit->nodes.user);
//   spn_user_node_t node = {
//     .ctx = unit,
//     .tag = spn_intern_cstr(tag),
//   };
//   sp_da_push(unit->nodes.user, node);
//
//   spn_node_t* out = SP_ALLOC(spn_node_t);
//   *out = (spn_node_t) {
//     .ctx = unit,
//     .index = index
//   };
//
//   return out;
// }
//
// void spn_node_add_input(spn_node_t* node, const c8* input) {
//   SPN_API_LOG(&node->ctx->ctx, "spn_node_add_input", "{}, {}", SP_FMT_STR(spn_find_user_node(node)->tag), SP_FMT_CSTR(input));
//   spn_user_node_t* info = spn_find_user_node(node);
//   sp_da_push(info->inputs, spn_intern_cstr(input));
// }
//
// void spn_node_add_output(spn_node_t* node, const c8* output) {
//   SPN_API_LOG(&node->ctx->ctx, "spn_node_add_output", "{}, {}", SP_FMT_STR(spn_find_user_node(node)->tag), SP_FMT_CSTR(output));
//   spn_user_node_t* info = spn_find_user_node(node);
//   sp_da_push(info->outputs, spn_intern_cstr(output));
// }
//
// void spn_node_link(spn_node_t* from, spn_node_t* to) {
//   SPN_API_LOG(&to->ctx->ctx, "spn_node_link", "{} -> {}", SP_FMT_STR(spn_find_user_node(from)->tag), SP_FMT_STR(spn_find_user_node(to)->tag));
//   spn_user_node_t* info = spn_find_user_node(to);
//   sp_da_push(info->deps, from);
// }
//
// void spn_node_set_fn(spn_node_t* node, spn_node_fn_t fn) {
//   SPN_API_LOG(&node->ctx->ctx, "spn_node_set_fn", "{}", SP_FMT_STR(spn_find_user_node(node)->tag));
//   spn_user_node_t* info = spn_find_user_node(node);
//   info->fn = fn;
// }
//
// void spn_node_set_user_data(spn_node_t* node, void* user_data) {
//   spn_user_node_t* info = spn_find_user_node(node);
//   info->user_data = user_data;
// }
//
// spn_build_ctx_t* spn_node_ctx_get_build(spn_node_ctx_t* ctx) {
//   return ctx->build;
// }
//
// void* spn_node_ctx_get_user_data(spn_node_ctx_t* ctx) {
//   return ctx->user_data;
// }
//
// spn_pkg_t* spn_get_pkg(spn_build_ctx_t* b) {
//   return b->pkg;
// }
//
// spn_profile_t* spn_get_profile(spn_build_ctx_t* b) {
//   return &b->session->profile;
// }
//
// spn_linkage_t spn_get_linkage(spn_build_ctx_t* b) {
//   return b->linkage;
// }
//
// spn_target_t* spn_get_target(spn_build_ctx_t* b, const c8* name) {
//   return spn_pkg_get_target(b->pkg, name);
// }
//
// const spn_build_ctx_t* spn_get_dep(spn_build_ctx_t* b, const c8* name) {
//   sp_str_t key = spn_intern_cstr(name);
//
//   if (sp_str_om_has(b->session->units.packages, key)) {
//     spn_pkg_unit_t* unit = sp_str_om_get(b->session->units.packages, key);
//     return &unit->ctx;
//   }
//
//   sp_ht_for(b->pkg->deps, it) {
//     sp_str_t k = *sp_ht_it_getkp(b->pkg->deps, it);
//     if (sp_str_equal(spn_qualified_name_to_pkg_id(k).name, key)) {
//       if (sp_str_om_has(b->session->units.packages, k)) {
//         spn_pkg_unit_t* unit = sp_str_om_get(b->session->units.packages, k);
//         return &unit->ctx;
//       }
//     }
//   }
//
//   return SP_NULLPTR;
// }
//
// const c8* spn_get_dir(const spn_build_ctx_t* b, spn_pkg_dir_t kind) {
//   return sp_str_to_cstr(spn_build_ctx_get_dir(b, kind));
// }
//
// const c8* spn_get_subdir(const spn_build_ctx_t* b, spn_pkg_dir_t kind, const c8* path) {
//   sp_str_t result = sp_fs_join_path(spn_build_ctx_get_dir(b, kind), sp_str_view(path));
//   return sp_str_to_cstr(result);
// }
//
// // @spader
// void spn_log(spn_build_ctx_t* ctx, const c8* message) {
//   spn_event_buffer_push_ex(spn.events, ctx->pkg, &ctx->logs, (spn_build_event_t) {
//     .kind = SPN_EVENT_USER_LOG,
//     .user_log = { .message = sp_str_view(message) },
//   });
// }
//
// void spn_copy(spn_build_ctx_t* build, spn_pkg_dir_t from_kind, const c8* from_path, spn_pkg_dir_t to_kind, const c8* to_path) {
//   sp_str_t from = sp_fs_join_path(spn_build_ctx_get_dir(build, from_kind), sp_str_view(from_path));
//   sp_str_t to = sp_fs_join_path(spn_build_ctx_get_dir(build, to_kind), sp_str_view(to_path));
//   SPN_API_LOG(build, "spn_copy", "{} -> {}", SP_FMT_STR(from), SP_FMT_STR(to));
//   sp_fs_copy(from, to);
// }
//
// void spn_write_file(spn_build_ctx_t* build, const c8* path, const c8* content) {
//   SPN_API_LOG(build, "spn_write_file", "{}", SP_FMT_CSTR(path));
//   sp_str_t full_path = sp_fs_join_path(spn_build_ctx_get_dir(build, SPN_DIR_WORK), sp_str_view(path));
//   sp_str_t parent = sp_fs_parent_path(full_path);
//   if (!sp_str_empty(parent)) {
//     sp_fs_create_dir(parent);
//   }
//
//   sp_io_writer_t io = sp_io_writer_from_file(full_path, SP_IO_WRITE_MODE_OVERWRITE);
//   sp_io_write_cstr(&io, content);
//   sp_io_writer_close(&io);
// }
//
// void spn_add_include(spn_build_ctx_t* b, spn_pkg_dir_t dir, const c8* path) {
//   SPN_API_LOG(b, "spn_add_include", "kind={}, {}", SP_FMT_S32(dir), SP_FMT_CSTR(path));
//   spn_pkg_add_include_ex(b->pkg, spn_build_ctx_resolve_dir(b, dir, sp_str_view(path)));
// }
//
// void spn_add_define(spn_build_ctx_t* b, const c8* define) {
//   SPN_API_LOG(b, "spn_add_define", "{}", SP_FMT_CSTR(define));
//   spn_pkg_add_define(b->pkg, define);
// }
//
// void spn_add_system_dep(spn_build_ctx_t* b, const c8* dep) {
//   SPN_API_LOG(b, "spn_add_system_dep", "{}", SP_FMT_CSTR(dep));
//   spn_pkg_add_system_dep(b->pkg, dep);
// }
//
// void spn_add_linkage(spn_build_ctx_t* b, spn_linkage_t linkage) {
//   SPN_API_LOG(b, "spn_add_linkage", "kind={}", SP_FMT_S32(linkage));
//   spn_pkg_add_linkage(b->pkg, linkage);
// }
//
// spn_index_t* spn_add_index(spn_build_ctx_t* b, const c8* name, const c8* location) {
//   SPN_API_LOG(b, "spn_add_index", "{}, {}", SP_FMT_CSTR(name), SP_FMT_CSTR(location));
//   return spn_pkg_add_index(b->pkg, name, location);
// }
