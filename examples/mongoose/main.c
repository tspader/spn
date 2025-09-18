// An example client + server for a game which communicate using packed binary messages over WebSocket.
// Messages are just a tagged union
//


#define SP_IMPLEMENTATION
#include "sp.h"

#define MG_ENABLE_LOG 0
#include "mongoose.h"
#include "mongoose.c"

/////////////////
// GAME EVENTS //
/////////////////
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


////////////////
// GAME STATE //
////////////////
typedef struct {
  u32 player1_score;
  u32 player2_score;
  u32 winner_id;
  bool running;
} spn_mg_game_state_t;


/////////
// APP //
/////////
typedef struct {
  struct mg_mgr mgr;
  spn_mg_game_state_t state;
  struct mg_connection* conn;
  u32 round;
} spn_mg_context_t;

typedef struct {
  spn_mg_context_t client;
  spn_mg_context_t server;
  sp_semaphore_t semaphore;
} spn_mg_app_t;

spn_mg_app_t app;

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

void spn_mg_server_handler(struct mg_connection* c, int ev, void* ev_data) {
  switch (ev) {
    case MG_EV_ACCEPT: {
      SP_LOG("Server: client accepted");
      break;
    }

    case MG_EV_HTTP_MSG: {
      struct mg_http_message* hm = (struct mg_http_message*)ev_data;
      if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
        mg_ws_upgrade(c, hm, NULL);
        SP_LOG("Server: WebSocket upgraded");
        app.server.conn = c;
      } else {
        mg_http_reply(c, 404, "", "Not Found\n");
      }
      break;
    }

    case MG_EV_WS_MSG: {
      struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
      sp_str_t data = {.data = (c8*)wm->data.buf, .len = (u32)wm->data.len};
      SP_LOG("Server: received message: {}", SP_FMT_STR(data));

      // Simulate game round
      app.server.round++;
      app.server.state.player1_score += 10;
      app.server.state.player2_score += 5;

      SP_LOG("Server: round {} - updating scores", SP_FMT_U32(app.server.round));

      // Send score updates
      spn_mg_message_t msg = {
        .type = SPN_MG_UPDATE_SCORE,
        .score_update = {.player_id = 1, .score = app.server.state.player1_score}
      };
      spn_mg_send_message(c, &msg);

      msg.score_update.player_id = 2;
      msg.score_update.score = app.server.state.player2_score;
      spn_mg_send_message(c, &msg);

      // Check for winner after 3 rounds
      if (app.server.round >= 3) {
        msg.type = SPN_MG_DECLARE_WINNER;
        msg.winner.player_id = 1;
        spn_mg_send_message(c, &msg);
        app.server.state.winner_id = 1;
        SP_LOG("Server: game over, player 1 wins");
      }
      break;
    }

    case MG_EV_CLOSE: {
      SP_LOG("Server: client disconnected");
      app.server.conn = NULL;
      break;
    }
  }
}

void spn_mg_client_handler(struct mg_connection* c, int ev, void* ev_data) {
  switch (ev) {
    case MG_EV_CONNECT: {
      SP_LOG("Client: TCP connected");
      break;
    }

    case MG_EV_WS_OPEN: {
      SP_LOG("Client: WebSocket opened, starting game");
      app.client.conn = c;
      // Send first play message
      sp_str_t msg = SP_CSTR("play");
      mg_ws_send(c, msg.data, msg.len, WEBSOCKET_OP_TEXT);
      break;
    }

    case MG_EV_WS_MSG: {
      struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
      sp_str_t data = {.data = (c8*)wm->data.buf, .len = (u32)wm->data.len};

      spn_mg_message_t msg = spn_mg_parse_message(data);

      switch (msg.type) {
        case SPN_MG_UPDATE_SCORE: {
          if (msg.score_update.player_id == 1) {
            app.client.state.player1_score = msg.score_update.score;
          } else if (msg.score_update.player_id == 2) {
            app.client.state.player2_score = msg.score_update.score;
          }

          SP_LOG("Client: Player {} score = {}",
            SP_FMT_U32(msg.score_update.player_id),
            SP_FMT_S32(msg.score_update.score)
          );

          // Wait a bit then play next round if no winner yet
          if (app.client.state.winner_id == 0) {
            usleep(500000); // 500ms
            sp_str_t play = SP_CSTR("play");
            SP_LOG("Client: requesting next round");
            mg_ws_send(c, play.data, play.len, WEBSOCKET_OP_TEXT);
          }
          break;
        }

        case SPN_MG_DECLARE_WINNER: {
          app.client.state.winner_id = msg.winner.player_id;
          SP_LOG("Client: Player {} wins! Final: P1={} P2={}",
            SP_FMT_U32(msg.winner.player_id),
            SP_FMT_U32(app.client.state.player1_score),
            SP_FMT_U32(app.client.state.player2_score)
          );
          app.client.state.running = false;
          break;
        }

        default:
          break;
      }
      break;
    }

    case MG_EV_ERROR: {
      SP_LOG("Client error: {}", SP_FMT_CSTR((c8*)ev_data));
      app.client.state.running = false;
      break;
    }

    case MG_EV_CLOSE: {
      SP_LOG("Client: connection closed");
      app.client.state.running = false;
      break;
    }
  }
}

s32 spn_mg_server_thread(void* user_data) {
  mg_mgr_init(&app.server.mgr);

  mg_http_listen(&app.server.mgr, "http://0.0.0.0:8080", spn_mg_server_handler, NULL);
  SP_LOG("Server: listening on port 8080");

  sp_semaphore_signal(&app.semaphore);

  while (app.server.state.running) {
    mg_mgr_poll(&app.server.mgr, 100);
  }

  mg_mgr_free(&app.server.mgr);
  SP_LOG("Server: shutting down");
  return 0;
}

s32 spn_mg_client_thread(void* user_data) {
  sp_semaphore_wait(&app.semaphore);

  // Let server fully start
  usleep(200000); // 200ms

  mg_mgr_init(&app.client.mgr);

  SP_LOG("Client: connecting to ws://127.0.0.1:8080/ws");
  struct mg_connection* conn = mg_ws_connect(&app.client.mgr, "ws://127.0.0.1:8080/ws",
                                              spn_mg_client_handler, NULL, NULL);

  if (!conn) {
    SP_LOG("Client: failed to create connection");
    mg_mgr_free(&app.client.mgr);
    return 1;
  }

  // Run client for up to 10 seconds or until game ends
  for (int i = 0; i < 100 && app.client.state.running; i++) {
    mg_mgr_poll(&app.client.mgr, 100);
  }

  mg_mgr_free(&app.client.mgr);
  SP_LOG("Client: shutting down");
  return 0;
}

s32 main(s32 num_args, const c8** args) {
  sp_init((sp_config_t){
    .allocator = sp_allocator_default()
  });

  SP_LOG("Starting WebSocket game example");

  app = (spn_mg_app_t) {
    .client = {
      .state = {
        .running = true
      }
    },
    .server = {
      .state = {
        .running = true
      }
    },
  };
  sp_semaphore_init(&app.semaphore);

  sp_thread_t server, client;
  sp_thread_init(&server, spn_mg_server_thread, NULL);
  sp_thread_init(&client, spn_mg_client_thread, NULL);

  sp_thread_join(&client);
  app.server.state.running = false;
  sp_thread_join(&server);

  sp_semaphore_destroy(&app.semaphore);

  SP_LOG("Game session completed");
  return 0;
}
