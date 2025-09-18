#define SP_IMPLEMENTATION
#include "sp.h"

#define MG_ENABLE_LOG 0
#include "mongoose.h"
#include "mongoose.c"

typedef enum {
  SPN_MG_UPDATE_SCORE,
  SPN_MG_DECLARE_WINNER,
} spn_mg_event_type_t;

typedef struct {
  u32 player_id;
  s32 score;
} spn_mg_score_update_t;

typedef struct {
  u32 player_id;
} spn_mg_winner_t;

typedef struct {
  spn_mg_event_type_t type;
  union {
    spn_mg_score_update_t score_update;
    spn_mg_winner_t winner;
  };
} spn_mg_message_t;

typedef struct {
  u32 player1_score;
  u32 player2_score;
  u32 winner_id;
} spn_mg_game_state_t;

spn_mg_game_state_t server_state = {0};
spn_mg_game_state_t client_state = {0};

void spn_mg_send_message(struct mg_connection* c, spn_mg_message_t* msg) {
  sp_str_t data = sp_format("{} {} {}",
    SP_FMT_U32(msg->type),
    SP_FMT_U32(msg->type == SPN_MG_UPDATE_SCORE ? msg->score_update.player_id : msg->winner.player_id),
    SP_FMT_S32(msg->type == SPN_MG_UPDATE_SCORE ? msg->score_update.score : 0)
  );
  mg_ws_send(c, data.data, data.len, WEBSOCKET_OP_TEXT);
}

spn_mg_message_t spn_mg_parse_message(sp_str_t data) {
  spn_mg_message_t msg = SP_ZERO_STRUCT(spn_mg_message_t);

  u32 values[3] = {0};
  u32 count = 0;

  sp_str_t remaining = data;
  while (remaining.len > 0 && count < 3) {
    u32 i = 0;
    while (i < remaining.len && remaining.data[i] != ' ') i++;

    sp_str_t token = {.data = remaining.data, .len = i};
    values[count++] = sp_parse_u32(token);

    if (i < remaining.len) {
      remaining.data += i + 1;
      remaining.len -= i + 1;
    } else {
      break;
    }
  }

  if (count >= 2) {
    msg.type = values[0];
    switch (msg.type) {
      case SPN_MG_UPDATE_SCORE:
        msg.score_update.player_id = values[1];
        if (count >= 3) {
          msg.score_update.score = (s32)values[2];
        }
        break;
      case SPN_MG_DECLARE_WINNER:
        msg.winner.player_id = values[1];
        break;
    }
  }

  return msg;
}

void spn_mg_handler(struct mg_connection* c, int ev, void* ev_data) {
  switch (ev) {
    case MG_EV_ACCEPT: {
      SP_LOG("Server: client accepted");
      break;
    }

    case MG_EV_HTTP_MSG: {
      struct mg_http_message* hm = (struct mg_http_message*)ev_data;

      // Server: handle WebSocket upgrade
      if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
        mg_ws_upgrade(c, hm, NULL);
        SP_LOG("Server: WebSocket upgraded");

        // After upgrade, immediately send initial game state
        spn_mg_message_t msg = {
          .type = SPN_MG_UPDATE_SCORE,
          .score_update = {.player_id = 1, .score = 0}
        };
        spn_mg_send_message(c, &msg);

        msg.score_update.player_id = 2;
        msg.score_update.score = 0;
        spn_mg_send_message(c, &msg);
      } else {
        // Serve a simple test page
        mg_http_reply(c, 200, "", "WebSocket Game Server Running\n");
      }
      break;
    }

    case MG_EV_WS_MSG: {
      struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
      sp_str_t data = {.data = (c8*)wm->data.buf, .len = (u32)wm->data.len};
      SP_LOG("Server: received {}", SP_FMT_STR(data));

      // Simulate game logic
      server_state.player1_score += 10;
      server_state.player2_score += 5;

      // Send score updates
      spn_mg_message_t msg = {
        .type = SPN_MG_UPDATE_SCORE,
        .score_update = {.player_id = 1, .score = server_state.player1_score}
      };
      spn_mg_send_message(c, &msg);

      msg.score_update.player_id = 2;
      msg.score_update.score = server_state.player2_score;
      spn_mg_send_message(c, &msg);

      // Check for winner
      if (server_state.player1_score >= 30) {
        msg.type = SPN_MG_DECLARE_WINNER;
        msg.winner.player_id = 1;
        spn_mg_send_message(c, &msg);
        server_state.winner_id = 1;
        SP_LOG("Server: game over, player 1 wins");
      }
      break;
    }
  }
}

s32 main(s32 num_args, const c8** args) {
  sp_init((sp_config_t){
    .allocator = sp_allocator_default()
  });

  struct mg_mgr mgr;
  mg_mgr_init(&mgr);

  // Start server
  mg_http_listen(&mgr, "http://0.0.0.0:8080", spn_mg_handler, NULL);
  SP_LOG("Server: listening on port 8080");
  SP_LOG("Open http://localhost:8080 in browser or use WebSocket client");
  SP_LOG("WebSocket endpoint: ws://localhost:8080/ws");

  // Run server
  SP_LOG("Server: running (press Ctrl+C to stop)");
  for (int i = 0; i < 100; i++) {
    mg_mgr_poll(&mgr, 100);
  }

  mg_mgr_free(&mgr);
  SP_LOG("Server: stopped");
  return 0;
}