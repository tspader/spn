#include "raylib.h"

int main(void) {
  InitWindow(800, 450, "spn + raylib");
  SetTargetFPS(60);

  Vector2 position = { 400, 225 };
  Vector2 velocity = { 4, 3 };

  while (!WindowShouldClose()) {
    position.x += velocity.x;
    position.y += velocity.y;
    if (position.x < 40 || position.x > 760) velocity.x = -velocity.x;
    if (position.y < 40 || position.y > 410) velocity.y = -velocity.y;

    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawCircleV(position, 40, MAROON);
    DrawText("built with spn", 20, 20, 30, DARKGRAY);
    DrawFPS(700, 20);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
