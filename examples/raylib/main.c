#define SP_IMPLEMENTATION
#include "sp.h"

#include <raylib.h>

typedef struct {
  Vector2 size;
  Vector2 center;
  f32 rotation;
  f32 speed;
  Color clear;
} spn_raylib_scene_t;

static spn_raylib_scene_t spn_raylib_scene_init(void) {
  spn_raylib_scene_t scene = {0};
  scene.size = (Vector2){280.0f, 180.0f};
  scene.center = (Vector2){0.0f, 0.0f};
  scene.rotation = 0.0f;
  scene.speed = 45.0f;
  scene.clear = (Color){18, 23, 33, 255};
  return scene;
}

static void spn_raylib_scene_update(spn_raylib_scene_t* scene, f32 dt, Vector2 canvas) {
  scene->rotation += scene->speed * dt;
  if (scene->rotation >= 360.0f) {
    scene->rotation -= 360.0f;
  }
  scene->center = (Vector2){canvas.x * 0.5f, canvas.y * 0.5f};
}

static void spn_raylib_scene_draw(const spn_raylib_scene_t* scene) {
  Rectangle rect = {
    scene->center.x - scene->size.x * 0.5f,
    scene->center.y - scene->size.y * 0.5f,
    scene->size.x,
    scene->size.y
  };
  Vector2 origin = {scene->size.x * 0.5f, scene->size.y * 0.5f};
  DrawCircleGradient((s32)scene->center.x, (s32)scene->center.y, 140.0f, (Color){59, 109, 235, 255}, (Color){59, 109, 235, 0});
  DrawRectangleRounded(rect, 0.12f, 6, (Color){240, 95, 48, 200});
  DrawRectangleRoundedLines(rect, 0.12f, 6, (Color){240, 195, 48, 255});
  DrawRectanglePro(rect, origin, scene->rotation, (Color){48, 173, 240, 220});
  DrawText("spn + raylib", (s32)(scene->center.x - 96.0f), (s32)(scene->center.y - 8.0f), 24, RAYWHITE);
}

s32 main(void) {
  sp_init_default();
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  const s32 width = 960;
  const s32 height = 540;
  InitWindow(width, height, "spn raylib");
  SetTargetFPS(60);

  spn_raylib_scene_t scene = spn_raylib_scene_init();
  SP_LOG("booted raylib {:fg brightcyan}", SP_FMT_CSTR(RAYLIB_VERSION));
  SP_LOG("upstream examples vendored in deps/raylib/vendor/examples");

  while (!WindowShouldClose()) {
    const f32 dt = GetFrameTime();
    const Vector2 canvas = { (f32)GetScreenWidth(), (f32)GetScreenHeight() };
    spn_raylib_scene_update(&scene, dt, canvas);

    BeginDrawing();
    ClearBackground(scene.clear);
    spn_raylib_scene_draw(&scene);
    DrawFPS(12, 12);
    EndDrawing();
  }

  CloseWindow();
  SP_LOG("raylib window closed");
  return 0;
}
