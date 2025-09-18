#define SP_IMPLEMENTATION
#include "sp.h"

#define MG_ENABLE_LOG 0
#include "mongoose.h"
#include "mongoose.c"

#include <unistd.h>

typedef struct {
  struct mg_mgr mgr;
  struct mg_connection *conn;
  bool is_connected;
  bool is_running;
  u32 msg_count;
} spn_ws_context_t;

static sp_semaphore_t server_ready;

static void server_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
      SP_LOG("Server: WebSocket client connected");
    }
  } else if (ev == MG_EV_WS_MSG) {
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    SP_LOG("Server received: {}B message", SP_FMT_U32((u32)wm->data.len));

    const c8* response = "pong from server";
    mg_ws_send(c, response, strlen(response), WEBSOCKET_OP_TEXT);
  }
}

static void client_handler(struct mg_connection *c, int ev, void *ev_data) {
  spn_ws_context_t *ctx = (spn_ws_context_t *)c->fn_data;

  if (ev == MG_EV_CONNECT) {
    struct mg_str host = mg_url_host("ws://127.0.0.1:8080");
    mg_printf(c,
              "GET /ws HTTP/1.1\r\n"
              "Host: %.*s\r\n"
              "Upgrade: websocket\r\n"
              "Connection: Upgrade\r\n"
              "Sec-WebSocket-Version: 13\r\n"
              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n",
              (int)host.len, host.buf);
  } else if (ev == MG_EV_WS_OPEN) {
    ctx->is_connected = true;
    SP_LOG("Client: WebSocket connection established");

    // Send first message immediately
    const c8* msg = "ping #0 from client";
    mg_ws_send(c, msg, strlen(msg), WEBSOCKET_OP_TEXT);
    ctx->msg_count = 1;
  } else if (ev == MG_EV_WS_MSG) {
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    SP_LOG("Client received: {}B message", SP_FMT_U32((u32)wm->data.len));

    // Send next message
    if (ctx->msg_count < 5) {
      c8 msg[64];
      snprintf(msg, sizeof(msg), "ping #%u from client", ctx->msg_count++);
      mg_ws_send(c, msg, strlen(msg), WEBSOCKET_OP_TEXT);
    } else {
      mg_ws_send(c, "goodbye", 7, WEBSOCKET_OP_TEXT);
      ctx->is_running = false;
    }
  } else if (ev == MG_EV_ERROR) {
    SP_LOG("Client error: {}", SP_FMT_CSTR((c8 *)ev_data));
    ctx->is_connected = false;
  } else if (ev == MG_EV_CLOSE) {
    ctx->is_connected = false;
    ctx->is_running = false;
  }
}

static s32 server_thread(void *arg) {
  spn_ws_context_t *ctx = (spn_ws_context_t *)arg;

  mg_mgr_init(&ctx->mgr);
  mg_http_listen(&ctx->mgr, "http://127.0.0.1:8080", server_handler, NULL);
  SP_LOG("Server started on port 8080");

  sp_semaphore_signal(&server_ready);

  while (ctx->is_running) {
    mg_mgr_poll(&ctx->mgr, 100);
  }

  SP_LOG("Server shutting down");
  mg_mgr_free(&ctx->mgr);
  return 0;
}

static s32 client_thread(void *arg) {
  spn_ws_context_t *ctx = (spn_ws_context_t *)arg;

  sp_semaphore_wait(&server_ready);
  usleep(100000);  // 100ms delay

  mg_mgr_init(&ctx->mgr);
  ctx->conn = mg_ws_connect(&ctx->mgr, "ws://127.0.0.1:8080/ws", client_handler, ctx, NULL);

  if (!ctx->conn) {
    SP_LOG("Client: Failed to create connection");
    mg_mgr_free(&ctx->mgr);
    return 1;
  }

  while (ctx->is_running) {
    mg_mgr_poll(&ctx->mgr, 100);
    usleep(100000); // 100ms between polls
  }

  SP_LOG("Client shutting down");
  mg_mgr_free(&ctx->mgr);
  return 0;
}

s32 main(s32 num_args, const c8** args) {
  sp_init((sp_config_t) {
    .allocator = sp_allocator_default()
  });

  spn_ws_context_t server_ctx = { .is_running = true, .msg_count = 0 };
  spn_ws_context_t client_ctx = { .is_running = true, .is_connected = false, .msg_count = 0 };

  sp_semaphore_init(&server_ready);

  sp_thread_t server, client;
  sp_thread_init(&server, server_thread, &server_ctx);
  sp_thread_init(&client, client_thread, &client_ctx);

  sp_thread_join(&client);

  // Stop server after client finishes
  server_ctx.is_running = false;
  sp_thread_join(&server);

  SP_LOG("WebSocket demo completed");

  return 0;
}
