#include <stdio.h>

#include "flecs/flecs.h"

typedef struct {
  double x;
  double y;
} Position, Velocity;

static void Move(ecs_iter_t* it) {
  Position* p = ecs_field(it, Position, 0);
  const Velocity* v = ecs_field(it, Velocity, 1);

  for (int i = 0; i < it->count; i++) {
    p[i].x += v[i].x;
    p[i].y += v[i].y;
    printf("%s moved to (%.0f, %.0f)\n", ecs_get_name(it->world, it->entities[i]), p[i].x, p[i].y);
  }
}

int main(void) {
  ecs_world_t* world = ecs_init();

  ECS_COMPONENT(world, Position);
  ECS_COMPONENT(world, Velocity);
  ECS_SYSTEM(world, Move, EcsOnUpdate, Position, [in] Velocity);

  ecs_entity_t bob = ecs_entity(world, { .name = "Bob" });
  ecs_set(world, bob, Position, { 0, 0 });
  ecs_set(world, bob, Velocity, { 1, 2 });

  ecs_entity_t alice = ecs_entity(world, { .name = "Alice" });
  ecs_set(world, alice, Position, { 10, 10 });
  ecs_set(world, alice, Velocity, { -1, 0 });

  for (int frame = 0; frame < 3; frame++) {
    ecs_progress(world, 0);
  }

  return ecs_fini(world);
}
