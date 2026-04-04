#include "ctx/types.h"
#include "graph/types.h"
#include "unit/types.h"
#include "target/types.h"

#include "external/cc.h"
#include "enum/enum.h"
#include "event/event.h"
#include "toolchain/toolchain.h"
#include "task/build/build.h"

s32 compile_embed(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* unit = (spn_target_unit_t*)user_data;
  spn_target_t* target = unit->info;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_EMBED_START,
    .embed_start = { .num_files = sp_da_size(target->embed) },
  });

  sp_tm_timer_t timer = sp_tm_start_timer();

  spn_cc_embed_ctx_t embedder = SP_ZERO_INITIALIZE();
  spn_cc_embed_ctx_init(&embedder, unit->session->profile.os);

  sp_da_for(target->embed, it) {
    spn_embed_t embed = target->embed[it];
    sp_str_t symbol = embed.symbol;
    spn_embed_types_t types = embed.types;
    sp_io_reader_t io = SP_ZERO_INITIALIZE();

    if (sp_str_empty(types.data)) {
      types.data = spn_intern_cstr("unsigned char");
    }

    if (sp_str_empty(types.size)) {
      types.size = spn_intern_cstr("unsigned long long");
    }

    switch (embed.kind) {
      case SPN_EMBED_MEM: {
        io = sp_io_reader_from_mem(embed.memory.buffer, embed.memory.size);
        break;
      }
      case SPN_EMBED_FILE: {
        if (!sp_fs_exists(embed.file.path)) {
          spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
            .kind = SPN_EVENT_EMBED_FAILED,
            .embed_failed = { .path = embed.file.path, .error = sp_str_lit("file not found") },
          });
          return SPN_ERROR;
        }

        io = sp_io_reader_from_file(embed.file.path);

        if (sp_str_empty(symbol)) {
          symbol = spn_cc_symbol_from_embedded_file(embed.file.path);
        }
        break;
      }
      case SPN_EMBED_DIR: {
        sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(embed.dir.path);
        sp_da_for(entries, e) {
          if (!sp_fs_is_regular_file(entries[e].file_path)) continue;
          sp_str_t rel = sp_str_suffix(entries[e].file_path, entries[e].file_path.len - embed.dir.path.len - 1);
          sp_io_reader_t dir_io = sp_io_reader_from_file(entries[e].file_path);
          if (spn_cc_embed_ctx_add(&embedder, dir_io, spn_cc_symbol_from_embedded_file(rel), rel, types.data, types.size)) {
            spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
              .kind = SPN_EVENT_EMBED_FAILED,
              .embed_failed = { .path = entries[e].file_path, .error = sp_str_lit("embed add failed") },
            });
            return SPN_ERROR;
          }
        }
        continue;
      }
    }

    sp_str_t path = embed.kind == SPN_EMBED_FILE ? embed.file.path : sp_str_lit("");
    if (spn_cc_embed_ctx_add(&embedder, io, symbol, path, types.data, types.size)) {
      spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
        .kind = SPN_EVENT_EMBED_FAILED,
        .embed_failed = { .error = sp_str_lit("embed add failed") },
      });
      return SPN_ERROR;
    }
  }

  sp_str_t obj = get_embed_object_path(unit);
  sp_str_t hdr = get_embed_header_path(unit);

  if (spn_cc_embed_ctx_write(&embedder, obj, hdr)) {
    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_EMBED_FAILED,
      .embed_failed = { .error = sp_str_lit("embed write failed") },
    });
    return SPN_ERROR;
  }

  u64 elapsed = sp_tm_read_timer(&timer);
  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_EMBED_PASSED,
    .embed_passed = { .object_path = obj, .header_path = hdr, .time = elapsed },
  });

  return SPN_OK;
}
