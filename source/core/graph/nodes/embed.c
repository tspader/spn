#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "unit/types.h"
#include "target/types.h"

#include "dag/dag.h"
#include "external/cc.h"
#include "event/event.h"
#include "intern/intern.h"
#include "graph/build.h"
#include "paths/paths.h"
#include "unit/package.h"
#include "unit/unit.h"

static void embed_obs(sp_da(spn_dag_obs_t)* obs, spn_dag_obs_kind_t kind, spn_path_t path) {
  sp_da_push(*obs, ((spn_dag_obs_t) { .kind = kind, .path = path }));
}

s32 spn_embed_write(spn_target_unit_t* unit, spn_path_t object, spn_path_t header, sp_mem_t obs_mem, sp_da(spn_dag_obs_t)* obs) {
  spn_target_info_t* info = unit->info;
  sp_str_t obj = spn_path_str(&spn.roots, spn.mem, object);
  sp_str_t hdr = spn_path_str(&spn.roots, spn.mem, header);

  spn_pkg_unit_announce_compile(unit->pkg);

  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_EMBED_START,
    .pkg = unit->pkg->info->name,
    .embed_start = { .target = info->name, .num_files = sp_da_size(info->embed) },
  });

  sp_tm_timer_t timer = sp_tm_start_timer();

  spn_cc_embed_ctx_t embedder = sp_zero;
  spn_cc_embed_ctx_init(&embedder, spn.mem, unit->pkg->build->profile.os, unit->pkg->build->profile.arch);

  sp_da_for(info->embed, it) {
    spn_embed_t embed = info->embed[it];
    sp_str_t symbol = embed.symbol;
    spn_embed_types_t types = embed.types;
    sp_mem_buffer_t data = sp_zero;

    if (sp_str_empty(types.data)) {
      types.data = sp_str_lit("unsigned char");
    }

    if (sp_str_empty(types.size)) {
      types.size = sp_str_lit("unsigned long long");
    }

    switch (embed.kind) {
      case SPN_EMBED_FILE: {
        embed_obs(obs, SPN_DAG_OBS_FILE, embed.file.path);
        sp_str_t file = spn_path_str(&spn.roots, embedder.mem, embed.file.path);
        sp_str_t content = sp_zero;
        if (sp_io_read_file(embedder.mem, file, &content) != SP_OK) {
          spn_event_buffer_push(spn.events, (spn_event_t) {
            .kind = SPN_EVENT_EMBED_FAILED,
            .pkg = unit->pkg->info->name,
            .embed_failed = { .target = info->name, .path = file, .error = sp_str_lit("file not found") },
          });
          return SPN_ERROR;
        }

        data = (sp_mem_buffer_t) {
          .data = (u8*)(uintptr_t)content.data,
          .len = content.len,
          .capacity = content.len,
        };

        if (sp_str_empty(symbol)) {
          symbol = spn_cc_symbol_from_embedded_file(embedder.mem, spn_tree_rel(unit->pkg->paths.roots, embed.file.path).sub);
        }
        break;
      }
      case SPN_EMBED_DIR: {
        sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
        spn_path_t root = embed.dir.path;
        sp_str_t dir = spn_path_str(&spn.roots, scratch.mem, root);
        embed_obs(obs, SPN_DAG_OBS_ENUMERATION, root);
        sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(scratch.mem, dir);
        sp_da_for(entries, e) {
          sp_str_t rel = sp_str_suffix(entries[e].path, entries[e].path.len - dir.len - 1);
          if (entries[e].kind == SP_FS_KIND_DIR) {
            embed_obs(obs, SPN_DAG_OBS_ENUMERATION, spn_path_join(obs_mem, root, rel));
            continue;
          }
          if (!sp_fs_is_file(entries[e].path)) continue;
          embed_obs(obs, SPN_DAG_OBS_FILE, spn_path_join(obs_mem, root, rel));
          if (!sp_str_empty(embed.dir.dest)) {
            rel = sp_fs_join_path(embedder.mem, embed.dir.dest, rel);
          }
          sp_str_t content = sp_zero;
          spn_err_t err = SPN_OK;
          if (sp_io_read_file(embedder.mem, entries[e].path, &content) != SP_OK) {
            err = SPN_ERROR;
          } else {
            sp_mem_buffer_t entry_data = {
              .data = (u8*)(uintptr_t)content.data,
              .len = content.len,
              .capacity = content.len,
            };
            err = spn_cc_embed_ctx_add(&embedder, entry_data, spn_cc_symbol_from_embedded_file(embedder.mem, rel), rel, types.data, types.size);
          }
          if (err) {
            spn_event_buffer_push(spn.events, (spn_event_t) {
              .kind = SPN_EVENT_EMBED_FAILED,
              .pkg = unit->pkg->info->name,
              .embed_failed = { .target = info->name, .path = sp_str_copy(spn.mem, entries[e].path), .error = sp_str_lit("embed add failed") },
            });
            sp_mem_end_scratch(scratch);
            return SPN_ERROR;
          }
        }
        sp_mem_end_scratch(scratch);
        continue;
      }
    }

    sp_str_t path = embed.kind == SPN_EMBED_FILE ? spn_tree_rel(unit->pkg->paths.roots, embed.file.path).sub : sp_str_lit("");
    if (spn_cc_embed_ctx_add(&embedder, data, symbol, path, types.data, types.size)) {
      spn_event_buffer_push(spn.events, (spn_event_t) {
        .kind = SPN_EVENT_EMBED_FAILED,
        .pkg = unit->pkg->info->name,
        .embed_failed = { .target = info->name, .error = sp_str_lit("embed add failed") },
      });
      return SPN_ERROR;
    }
  }

  spn_err_t write_err = spn_cc_embed_ctx_write(&embedder, obj, hdr);
  spn_cc_embed_ctx_free(&embedder);
  if (write_err) {
    spn_event_buffer_push(spn.events, (spn_event_t) {
      .kind = SPN_EVENT_EMBED_FAILED,
      .pkg = unit->pkg->info->name,
      .embed_failed = { .target = info->name, .error = sp_str_lit("embed write failed") },
    });
    return SPN_ERROR;
  }

  u64 elapsed = sp_tm_read_timer(&timer);
  spn_event_buffer_push(spn.events, (spn_event_t) {
    .kind = SPN_EVENT_EMBED_PASSED,
    .pkg = unit->pkg->info->name,
    .embed_passed = { .target = info->name, .object_path = obj, .header_path = hdr, .time = elapsed },
  });

  return SPN_OK;
}
