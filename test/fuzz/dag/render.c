#include "fuzz.h"
#include "sp/io.h"

static void fz_line(sp_io_writer_t* io, sp_str_t line) {
  sp_io_write_str(io, line, SP_NULLPTR);
  sp_io_write_c8(io, '\n');
}

void fz_render_mermaid(sp_io_writer_t* io, fz_universe_t* u) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_mem_t mem = s.mem;

  sp_fmt_io(io, "flowchart TD\n");
  sp_fmt_io(io, "  classDef phantom stroke-dasharray: 4 4\n");

  sp_da_for(u->artifacts, it) {
    fz_artifact_t* artifact = &u->artifacts[it];
    switch (artifact->kind) {
      case FZ_ARTIFACT_VALUE: {
        sp_fmt_io(io, "  f{}([\"value f{} c{}\"])", sp_fmt_uint(it), sp_fmt_uint(it), sp_fmt_uint(artifact->content));
        sp_io_write_cstr(io, "\n", SP_NULLPTR);
        break;
      }
      case FZ_ARTIFACT_SOURCE: {
        sp_fmt_io(io, "  f{}[/\"{} c{}\"/]", sp_fmt_uint(it), sp_fmt_str(fz_artifact_path(mem, u, it)), sp_fmt_uint(artifact->content));
        sp_io_write_cstr(io, "\n", SP_NULLPTR);
        break;
      }
      case FZ_ARTIFACT_OUTPUT: {
        sp_fmt_io(io, "  f{}[\"{}\"]", sp_fmt_uint(it), sp_fmt_str(fz_artifact_path(mem, u, it)));
        sp_io_write_cstr(io, "\n", SP_NULLPTR);
        break;
      }
    }
  }

  sp_da_for(u->actions, at) {
    fz_action_t* action = &u->actions[at];
    sp_fmt_io(io, "  a{}[[\"a{} id{}{}\"]]\n",
      sp_fmt_uint(at), sp_fmt_uint(at), sp_fmt_uint(action->identity),
      sp_fmt_str(action->discover ? sp_str_lit(" discover") : sp_str_lit("")));
  }

  u64 phantom_count = u->profile.limits.phantoms;
  bool* phantoms = sp_alloc_n(mem, bool, phantom_count);
  sp_mem_zero(phantoms, phantom_count * sizeof(bool));
  sp_da_for(u->actions, at) {
    sp_da_for(u->actions[at].obs, ot) {
      fz_obs_t obs = u->actions[at].obs[ot];
      if (obs.probe) {
        phantoms[obs.phantom] = true;
      }
    }
  }
  sp_for(it, phantom_count) {
    if (!phantoms[it]) continue;
    sp_fmt_io(io, "  g{}((\"{}\")):::phantom", sp_fmt_uint(it), sp_fmt_str(fz_phantom_path(mem, it)));
    sp_io_write_cstr(io, "\n", SP_NULLPTR);
  }

  sp_da_for(u->actions, at) {
    fz_action_t* action = &u->actions[at];
    sp_da_for(action->consumes, ct) {
      sp_fmt_io(io, "  f{} --> a{}", sp_fmt_uint(action->consumes[ct]), sp_fmt_uint(at));
      sp_io_write_cstr(io, "\n", SP_NULLPTR);
    }
    sp_da_for(action->produces, pt) {
      sp_fmt_io(io, "  a{} --> f{}", sp_fmt_uint(at), sp_fmt_uint(action->produces[pt]));
      sp_io_write_cstr(io, "\n", SP_NULLPTR);
    }
    sp_da_for(action->obs, ot) {
      fz_obs_t obs = action->obs[ot];
      if (obs.probe) {
        sp_fmt_io(io, "  a{} -. probe .-> g{}", sp_fmt_uint(at), sp_fmt_uint(obs.phantom));
        sp_io_write_cstr(io, "\n", SP_NULLPTR);
      }
      else {
        sp_fmt_io(io, "  a{} -. obs .-> f{}", sp_fmt_uint(at), sp_fmt_uint(obs.artifact));
        sp_io_write_cstr(io, "\n", SP_NULLPTR);
      }
    }
  }

  sp_mem_end_scratch(s);
}

sp_str_t fz_render_iteration(sp_mem_t mem, sp_str_t root, fz_universe_t* u, fz_trace_t* trace, u64 iter) {
  sp_fs_create_dir(root);
  sp_str_t dir = sp_fs_join_path(mem, root, sp_fmt(mem, "{:0>3}", sp_fmt_uint(iter)).value);
  sp_fs_create_dir(dir);

  sp_io_file_writer_t repro = sp_zero;
  if (!sp_io_file_writer_from_path(&repro, sp_fs_join_path(mem, dir, sp_str_lit("repro")))) {
    fz_line(&repro.base, sp_fuzz_repro_args(mem, iter));
    sp_io_file_writer_close(&repro);
  }

  sp_io_file_writer_t graph = sp_zero;
  if (!sp_io_file_writer_from_path(&graph, sp_fs_join_path(mem, dir, sp_str_lit("graph.mmd")))) {
    fz_line(&graph.base, sp_fmt(mem, "%% iter {}: {} actions, {} artifacts{}{}{}",
      sp_fmt_uint(iter),
      sp_fmt_uint(sp_da_size(u->actions)),
      sp_fmt_uint(sp_da_size(u->artifacts)),
      sp_fmt_str(u->profile.big ? sp_str_lit(", big") : sp_str_lit("")),
      sp_fmt_str(u->cyclic ? sp_str_lit(", cyclic") : sp_str_lit("")),
      sp_fmt_str(!u->cyclic && u->obs_cyclic ? sp_str_lit(", obs-cyclic") : sp_str_lit(""))).value);
    fz_render_mermaid(&graph.base, u);
    sp_io_file_writer_close(&graph);
  }

  sp_io_file_writer_t steps = sp_zero;
  if (!sp_io_file_writer_from_path(&steps, sp_fs_join_path(mem, dir, sp_str_lit("trace.txt")))) {
    sp_da_for(trace->steps, st) {
      fz_step_t* step = &trace->steps[st];
      switch (step->kind) {
        case FZ_STEP_RUN: {
          fz_line(&steps.base, sp_str_lit("run"));
          break;
        }
        case FZ_STEP_MUTATE: {
          fz_line(&steps.base, sp_fmt(mem, "mutate f{} c{}", sp_fmt_uint(step->artifact), sp_fmt_uint(step->content)).value);
          break;
        }
        case FZ_STEP_TOUCH: {
          fz_line(&steps.base, sp_fmt(mem, "touch f{}", sp_fmt_uint(step->artifact)).value);
          break;
        }
        case FZ_STEP_REVERT: {
          fz_line(&steps.base, sp_fmt(mem, "revert f{} c{}", sp_fmt_uint(step->artifact), sp_fmt_uint(step->content)).value);
          break;
        }
        case FZ_STEP_STEALTH: {
          fz_line(&steps.base, sp_fmt(mem, "stealth f{} c{}", sp_fmt_uint(step->artifact), sp_fmt_uint(step->content)).value);
          break;
        }
        case FZ_STEP_DELETE: {
          fz_line(&steps.base, sp_fmt(mem, "delete f{}", sp_fmt_uint(step->artifact)).value);
          break;
        }
        case FZ_STEP_PHANTOM: {
          fz_line(&steps.base, sp_fmt(mem, "phantom g{} c{}", sp_fmt_uint(step->artifact), sp_fmt_uint(step->content)).value);
          break;
        }
        case FZ_STEP_DISCOVERY: {
          fz_line(&steps.base, sp_str_lit("discovery"));
          break;
        }
        case FZ_STEP_EIO: {
          fz_line(&steps.base, sp_fmt(mem, "eio 1/{}", sp_fmt_uint(step->content)).value);
          break;
        }
        case FZ_STEP_CRASH: {
          fz_line(&steps.base, sp_str_lit("crash"));
          break;
        }
        case FZ_STEP_BLOB: {
          fz_line(&steps.base, sp_str_lit("blob"));
          break;
        }
        case FZ_STEP_EVICT: {
          fz_line(&steps.base, sp_str_lit("evict"));
          break;
        }
        case FZ_STEP_COUNT: {
          break;
        }
      }
    }
    sp_io_file_writer_close(&steps);
  }

  return dir;
}
