/*
 * Copyright 2022 Nicolas Maltais
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tworld_actor.h"
#include "tworld.h"

#define DIRECTION_MASK 0x3

actor_t actor_create(entity_t entity, direction_t direction) {
    return entity | direction;
}

entity_t actor_get_entity(actor_t actor) {
    return actor & ~DIRECTION_MASK;
}

direction_t actor_get_direction(actor_t actor) {
    return actor & DIRECTION_MASK;
}

actor_t actor_with_direction(actor_t actor, direction_t direction) {
    return (actor & ~DIRECTION_MASK) | direction;
}

actor_t actor_with_entity(actor_t actor, entity_t entity) {
    return entity | (actor & DIRECTION_MASK);
}

actor_t actor_reverse_tank(actor_t actor) {
    return actor ^ 0x04;
}

bool actor_is_tank(actor_t actor) {
    return (actor & 0x38) == 0x30;
}

bool actor_is_block(actor_t actor) {
    return actor >= 0x0f && actor <= 0x17;
}

bool actor_is_monster(actor_t actor) {
    return actor >= ENTITY_BUG;
}

bool actor_is_monster_or_block(actor_t actor) {
    return actor >= ENTITY_BLOCK_GHOST;
}

bool actor_is_on_actor_list(actor_t actor) {
    return actor >= ENTITY_BLOCK;
}

grid_pos_t act_actor_get_x(active_actor_t a) {
    return a & 0x1f;
}

grid_pos_t act_actor_get_y(active_actor_t a) {
    return (a >> 7) & 0x1f;
}

position_t act_actor_get_pos(active_actor_t a) {
    return (position_t) {act_actor_get_x(a), act_actor_get_y(a)};
}

step_t act_actor_get_step(active_actor_t a) {
    return (step_t) ((step_t) (a >> 12) - STEP_BIAS);
}

actor_state_t act_actor_get_state(active_actor_t a) {
    return (uint8_t) a & ACTOR_STATE_MASK;
}
