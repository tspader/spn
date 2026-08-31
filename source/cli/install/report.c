#include "install/install.h"

typedef struct {
  bool path;
  bool rc;
} stuck_t;

static stuck_t classify_stuck(spn_install_plan_t* plan, spn_install_result_t* result) {
  stuck_t stuck = sp_zero;
  sp_for(it, result->num_stuck) {
    switch (plan->path[result->stuck[it]].role) {
      case SPN_INSTALL_ROLE_NONE: break;
      case SPN_INSTALL_ROLE_PATH: stuck.path = true; break;
      case SPN_INSTALL_ROLE_RC: stuck.rc = true; break;
      case SPN_INSTALL_ROLE_FISH: break;
    }
  }
  return stuck;
}

static void push(spn_install_msgs_t* msgs, spn_install_msg_t msg) {
  SP_ASSERT(msgs->count < SPN_INSTALL_MAX_MSGS);
  msgs->items[msgs->count++] = msg;
}

spn_install_msgs_t spn_install_report(spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_plan_t* plan, spn_install_result_t* result) {
  spn_install_msgs_t msgs = sp_zero;
  push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_INSTALLED, .subject = layout->exe });

  stuck_t stuck = classify_stuck(plan, result);
  if (plan->state == SPN_INSTALL_PATH_MANUAL || stuck.path) {
    push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_MANUAL, .subject = layout->bin_native });
  }
  else if (plan->state == SPN_INSTALL_PATH_UPDATED && !stuck.rc) {
    switch (layout->os) {
      case SPN_INSTALL_OS_UNIX: {
        push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_RESTART_SHELL, .subject = layout->rc_line });
        break;
      }
      case SPN_INSTALL_OS_WINDOWS: {
        push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_RESTART_TERMINAL });
        break;
      }
    }
  }

  sp_for(it, result->num_stuck) {
    spn_install_action_t* action = &plan->path[result->stuck[it]];
    if (action->kind == SPN_INSTALL_ACTION_SET_USER_PATH) {
      push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_STUCK_REGISTRY });
    }
    else {
      push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_STUCK_FILE, .subject = action->path });
    }
  }
  if (stuck.rc) {
    push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_ADD_LINE, .subject = layout->rc_line });
  }

  if (!sp_str_empty(facts->shadow) && !spn_install_path_equal(layout->os, facts->shadow, layout->exe)) {
    push(&msgs, (spn_install_msg_t) { .kind = SPN_INSTALL_MSG_SHADOW, .subject = facts->shadow, .detail = layout->exe });
  }
  return msgs;
}
