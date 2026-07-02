#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "graph/types.h"
#include "unit/types.h"
#include "target/types.h"

#include "external/cc.h"
#include "event/event.h"
#include "intern/intern.h"
#include "task/build/build.h"
#include "unit/package.h"

s32 compile_embed(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* unit = (spn_target_unit_t*)user_data;
  spn_target_info_t* info = unit->info;

  spn_pkg_unit_announce_compile(unit->pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_EMBED_START,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .embed_start = { .num_files = sp_da_size(info->embed) },
  });

  sp_tm_timer_t timer = sp_tm_start_timer();

  spn_cc_embed_ctx_t embedder = SP_ZERO_INITIALIZE();
  spn_cc_embed_ctx_init(&embedder, spn.mem, unit->session->profile.os);

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
      case SPN_EMBED_MEM: {
        data = (sp_mem_buffer_t) {
          .data = (u8*)embed.memory.buffer,
          .len = embed.memory.size,
          .capacity = embed.memory.size,
        };
        break;
      }
      case SPN_EMBED_FILE: {
        sp_str_t content = sp_zero;
        if (sp_io_read_file(embedder.mem, embed.file.path, &content) != SP_OK) {
          spn_event_buffer_push(spn.events, (spn_build_event_t) {
            .kind = SPN_EVENT_EMBED_FAILED,
            .pkg = unit->pkg->info,
            .io = &unit->logs,
            .embed_failed = { .path = embed.file.path, .error = sp_str_lit("file not found") },
          });
          return SPN_ERROR;
        }

        data = (sp_mem_buffer_t) {
          .data = (u8*)(uintptr_t)content.data,
          .len = content.len,
          .capacity = content.len,
        };

        if (sp_str_empty(symbol)) {
          symbol = spn_cc_symbol_from_embedded_file(embedder.mem, embed.file.path);
        }
        break;
      }
      case SPN_EMBED_DIR: {
        sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
        sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(scratch.mem, embed.dir.path);
        sp_da_for(entries, e) {
          if (!sp_fs_is_file(entries[e].path)) continue;
          sp_str_t rel = sp_str_suffix(entries[e].path, entries[e].path.len - embed.dir.path.len - 1);
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
            spn_event_buffer_push(spn.events, (spn_build_event_t) {
              .kind = SPN_EVENT_EMBED_FAILED,
              .pkg = unit->pkg->info,
              .io = &unit->logs,
              .embed_failed = { .path = sp_str_copy(spn.mem, entries[e].path), .error = sp_str_lit("embed add failed") },
            });
            sp_mem_end_scratch(scratch);
            return SPN_ERROR;
          }
        }
        sp_mem_end_scratch(scratch);
        continue;
      }
    }

    sp_str_t path = embed.kind == SPN_EMBED_FILE ? embed.file.path : sp_str_lit("");
    if (spn_cc_embed_ctx_add(&embedder, data, symbol, path, types.data, types.size)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_EMBED_FAILED,
        .pkg = unit->pkg->info,
        .io = &unit->logs,
        .embed_failed = { .error = sp_str_lit("embed add failed") },
      });
      return SPN_ERROR;
    }
  }

  sp_str_t obj = get_embed_object_path(spn.mem, unit);
  sp_str_t hdr = get_embed_header_path(spn.mem, unit);

  spn_err_t write_err = spn_cc_embed_ctx_write(&embedder, obj, hdr);
  spn_cc_embed_ctx_free(&embedder);
  if (write_err) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_EMBED_FAILED,
      .pkg = unit->pkg->info,
      .io = &unit->logs,
      .embed_failed = { .error = sp_str_lit("embed write failed") },
    });
    return SPN_ERROR;
  }

  u64 elapsed = sp_tm_read_timer(&timer);
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_EMBED_PASSED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .embed_passed = { .object_path = obj, .header_path = hdr, .time = elapsed },
  });

  return SPN_OK;
}
