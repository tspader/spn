#include "app/app.h"
#include "ctx/ctx.h"
#include "graph/graph.h"
#include "session/session.h"
#include "external/cc.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/str.h"

spn_task_step_t spn_task_render_graph(spn_app_t* app) {
  return spn_task_done();
}
