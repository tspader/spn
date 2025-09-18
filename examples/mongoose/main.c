// An example client + server for a game which communicate using packed binary messages over WebSocket.
// Messages are just a tagged union
//


#define SP_IMPLEMENTATION
#include "sp.h"

#define MG_ENABLE_LOG 0
#include "mongoose.h"
#include "mongoose.c"

//////////////
// REQUESTS //
//////////////
typedef enum {
  SPN_MG_REQUEST_NONE,
  SPN_MG_REQUEST_PLAY,
  SPN_MG_REQUEST_TURN,
} spn_mg_request_kind_t;

typedef struct {
  spn_mg_request_kind_t kind;
}spn_mg_request_t;

////////////
// EVENTS //
////////////
typedef enum {
  SPN_MG_EVENT_UPDATE_SCORE,
  SPN_MG_EVENT_DECLARE_WINNER,
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
    spn_mg_score_update_t score;
    spn_mg_winner_t winner;
  };
} spn_mg_event_t;


////////////////
// GAME STATE //
////////////////
typedef enum {
  SPN_MG_PLAYER_NONE,
  SPN_MG_PLAYER_1,
  SPN_MG_PLAYER_2,
} spn_mg_player_id_t;

#define SPN_MG_MAX_ROUNDS 3

typedef struct {
  struct {
    u32 p1;
    u32 p2;
  } score;
  u32 winner_id;
} spn_mg_game_state_t;


/////////
// APP //
/////////
typedef struct {
  struct mg_mgr mgr;
  spn_mg_game_state_t state;
  struct mg_connection* conn;
  u32 round;
  bool running;
} spn_mg_context_t;

typedef struct {
  spn_mg_context_t client;
  spn_mg_context_t server;
  sp_semaphore_t semaphore;
  sp_mutex_t mutex;
} spn_mg_app_t;

spn_mg_app_t app;


////////////
// LOGGING //
////////////
void spn_mg_server_log(const c8* fmt, ...) {
  sp_mutex_lock(&app.mutex);
  va_list args;
  va_start(args, fmt);
  sp_str_t formatted = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);
  SP_LOG("{:fg brightcyan} {}", SP_FMT_CSTR("server"), SP_FMT_STR(formatted));
  sp_mutex_unlock(&app.mutex);

}

void spn_mg_client_log(const c8* fmt, ...) {
  sp_mutex_lock(&app.mutex);
  va_list args;
  va_start(args, fmt);
  sp_str_t formatted = sp_format_v(SP_CSTR(fmt), args);
  va_end(args);
  SP_LOG("{:fg brightgreen} {}", SP_FMT_CSTR("client"), SP_FMT_STR(formatted));
  sp_mutex_unlock(&app.mutex);
}


//////////////
// HANDLERS //
//////////////
void spn_mg_server_handler(struct mg_connection* c, int ev, void* ev_data) {
  switch (ev) {
    case MG_EV_ACCEPT: {
      spn_mg_server_log("Client accepted");
      break;
    }

    case MG_EV_HTTP_MSG: {
      struct mg_http_message* hm = (struct mg_http_message*)ev_data;
      if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
        mg_ws_upgrade(c, hm, NULL);
        spn_mg_server_log("WebSocket upgraded");
        app.server.conn = c;
      }
      else {
        mg_http_reply(c, 404, "", "Not Found\n");
      }
      break;
    }

    case MG_EV_WS_MSG: {
      struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;

      if ((wm->flags & 0x0F) == WEBSOCKET_OP_BINARY) {
        SP_ASSERT(wm->data.len == sizeof(spn_mg_request_t));
        spn_mg_request_t* request = (spn_mg_request_t*)wm->data.buf;

        switch (request->kind) {
          case SPN_MG_REQUEST_PLAY: {
            spn_mg_server_log("{:fg brightblack}", SP_FMT_CSTR("SPN_MG_REQUEST_PLAY"));
            app.server.round = 0;
            app.server.state.score.p1 = 0;
            app.server.state.score.p2 = 0;
            app.server.state.winner_id = 0;

            // Send initial score to start the game
            spn_mg_event_t msg;
            msg.type = SPN_MG_EVENT_UPDATE_SCORE;
            msg.score.player_id = SPN_MG_PLAYER_1;
            msg.score.score = 0;
            mg_ws_send(c, &msg, sizeof(msg), WEBSOCKET_OP_BINARY);

            msg.score.player_id = SPN_MG_PLAYER_2;
            msg.score.score = 0;
            mg_ws_send(c, &msg, sizeof(msg), WEBSOCKET_OP_BINARY);
            break;
          }
          case SPN_MG_REQUEST_TURN: {
            spn_mg_server_log("{:fg brightblack}", SP_FMT_CSTR("SPN_MG_REQUEST_TURN"));

            // Don't process turns after game is over
            if (app.server.state.winner_id != 0) {
              spn_mg_server_log("Game already finished, ignoring turn");
              break;
            }

            app.server.round++;
            app.server.state.score.p1 += 10;
            app.server.state.score.p2 += 5;

            spn_mg_event_t msg;
            msg.type = SPN_MG_EVENT_UPDATE_SCORE;
            msg.score.player_id = SPN_MG_PLAYER_1;
            msg.score.score = app.server.state.score.p1;
            mg_ws_send(c, &msg, sizeof(msg), WEBSOCKET_OP_BINARY);

            msg.score.player_id = SPN_MG_PLAYER_2;
            msg.score.score = app.server.state.score.p2;
            mg_ws_send(c, &msg, sizeof(msg), WEBSOCKET_OP_BINARY);

            if (app.server.round >= SPN_MG_MAX_ROUNDS) {
              // Determine winner based on actual scores
              u32 winner = (app.server.state.score.p1 > app.server.state.score.p2) ? SPN_MG_PLAYER_1 : SPN_MG_PLAYER_2;

              msg.type = SPN_MG_EVENT_DECLARE_WINNER;
              msg.winner.player_id = winner;
              mg_ws_send(c, &msg, sizeof(msg), WEBSOCKET_OP_BINARY);

              app.server.state.winner_id = winner;

              spn_mg_server_log("{:fg brightred}", SP_FMT_CSTR("game over"));
            }

            break;
          }
          default: {
            spn_mg_server_log("{:fg red}: {:fg brightblack}", SP_FMT_CSTR("unknown request"), SP_FMT_U32(request->kind));
            SP_UNREACHABLE_CASE();
          }
        }
      }

      break;
    }

    case MG_EV_CLOSE: {
      spn_mg_server_log("client disconnected");
      app.server.conn = NULL;
      break;
    }
  }
}

void spn_mg_client_handler(struct mg_connection* c, int ev, void* ev_data) {
  switch (ev) {
    case MG_EV_CONNECT: {
      spn_mg_client_log("TCP connected");
      break;
    }

    case MG_EV_WS_OPEN: {
      spn_mg_client_log("WebSocket opened, starting game");
      app.client.conn = c;

      spn_mg_request_t request = {
        .kind = SPN_MG_REQUEST_PLAY
      };
      mg_ws_send(c, &request, sizeof(request), WEBSOCKET_OP_BINARY);
      break;
    }

    case MG_EV_WS_MSG: {
      struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;

      if ((wm->flags & 0x0F) == WEBSOCKET_OP_BINARY) {
        SP_ASSERT(wm->data.len == sizeof(spn_mg_event_t));
        spn_mg_event_t message = *(spn_mg_event_t*)wm->data.buf;

        switch (message.type) {
          case SPN_MG_EVENT_UPDATE_SCORE: {
            if (message.score.player_id == 1) {
              app.client.state.score.p1 = message.score.score;
            }
            else if (message.score.player_id == 2) {
              app.client.state.score.p2 = message.score.score;

              // After receiving P2 score, send next turn if game not over
              if (app.client.state.winner_id == 0) {
                sp_os_sleep_ms(500);

                spn_mg_request_t request = {
                  .kind = SPN_MG_REQUEST_TURN
                };
                mg_ws_send(c, &request, sizeof(request), WEBSOCKET_OP_BINARY);
              }
            }

            spn_mg_client_log(
              "{:fg brightblack} P{} -> {}",
              SP_FMT_CSTR("SPN_MG_EVENT_UPDATE_SCORE"),
              SP_FMT_U32(message.score.player_id),
              SP_FMT_S32(message.score.score)
            );
            break;
          }

          case SPN_MG_EVENT_DECLARE_WINNER: {
            spn_mg_client_log(
              "{:fg brightblack} P1 ({}) P2 ({})",
              SP_FMT_CSTR("SPN_MG_EVENT_DECLARE_WINNER"),
              SP_FMT_U32(app.client.state.score.p1),
              SP_FMT_U32(app.client.state.score.p2)
            );

            app.client.state.winner_id = message.winner.player_id;
            app.client.running = false;
            break;
          }

          default:
            spn_mg_client_log("unknown message type {}", SP_FMT_U32(message.type));
            break;
        }
      }
      break;
    }

    case MG_EV_ERROR: {
      spn_mg_client_log("error: {}", SP_FMT_CSTR((c8*)ev_data));
      app.client.running = false;
      break;
    }

    case MG_EV_CLOSE: {
      spn_mg_client_log("connection closed");
      app.client.running = false;
      break;
    }
  }
}


/////////////
// THREADS //
/////////////
s32 spn_mg_server_thread(void* user_data) {
  mg_mgr_init(&app.server.mgr);

  mg_http_listen(&app.server.mgr, "http://0.0.0.0:8080", spn_mg_server_handler, NULL);
  spn_mg_server_log("Listening on port {:fg brightyellow}", SP_FMT_U32(8080));

  sp_semaphore_signal(&app.semaphore);

  while (app.server.running) {
    mg_mgr_poll(&app.server.mgr, 100);
  }

  mg_mgr_free(&app.server.mgr);
  spn_mg_server_log("shutting down");
  return 0;
}

s32 spn_mg_client_thread(void* user_data) {
  sp_semaphore_wait(&app.semaphore);

  // Let server fully start
  sp_os_sleep_ms(200.0);

  mg_mgr_init(&app.client.mgr);

  spn_mg_client_log(
    "connecting to {:fg brightyellow}",
    SP_FMT_CSTR("ws://127.0.0.1:8080/ws")
  );
  struct mg_connection* conn = mg_ws_connect(
    &app.client.mgr,
    "ws://127.0.0.1:8080/ws",
    spn_mg_client_handler, NULL, NULL
  );

  if (!conn) {
    spn_mg_client_log("failed to create connection");
    mg_mgr_free(&app.client.mgr);
    return 1;
  }

  // Poll for game updates from the server
  while (app.client.running) {
    mg_mgr_poll(&app.client.mgr, 100);
  }

  mg_mgr_free(&app.client.mgr);
  spn_mg_client_log("shutting down");
  return 0;
}


//////////
// MAIN //
//////////
s32 main(s32 num_args, const c8** args) {
  sp_init((sp_config_t){
    .allocator = sp_allocator_default()
  });

  SP_LOG("{:fg brightgreen} starting mongoose demo", SP_FMT_CSTR("main  "));
  app.client.running = true;
  app.server.running = true;

  sp_semaphore_init(&app.semaphore);
  sp_mutex_init(&app.mutex, SP_MUTEX_PLAIN);

  sp_thread_t server, client;
  sp_thread_init(&server, spn_mg_server_thread, NULL);
  sp_thread_init(&client, spn_mg_client_thread, NULL);

  sp_thread_join(&client);
  app.server.running = false;
  sp_thread_join(&server);

  sp_semaphore_destroy(&app.semaphore);
  sp_mutex_destroy(&app.mutex);

  return 0;
}
