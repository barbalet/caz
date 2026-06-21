#include "caz_env.h"

#include "../../src/caz_droid.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAZ_ENV_VM_INSTRUCTIONS_PER_TICK 24u
#define CAZ_ENV_JUNCTION_CONTACT_FT 0.95f
#define CAZ_ENV_JUNCTION_CLAW_REACH_FT 2.00f

static const CazProgramMetadata *program_metadata_for(uint8_t program)
{
    const CazProgramMetadata *metadata = caz_loader_program_metadata((CazProgramKind)program);
    return metadata != NULL ? metadata : caz_loader_program_metadata(CAZ_PROGRAM_RETURN_TO_CHARGE);
}

static int assignable_program_count(void)
{
    int count = 0;
    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        const CazProgramMetadata *metadata = caz_loader_program_metadata_at(index);
        if (metadata != NULL && metadata->cazenv_assignable) {
            count++;
        }
    }
    return count;
}

static uint8_t assignable_program_for_droid(int droid_index)
{
    const int count = assignable_program_count();
    const int target = count > 0 ? droid_index % count : 0;
    int seen = 0;
    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        const CazProgramMetadata *metadata = caz_loader_program_metadata_at(index);
        if (metadata == NULL || !metadata->cazenv_assignable) {
            continue;
        }
        if (seen == target) {
            return (uint8_t)metadata->kind;
        }
        seen++;
    }
    return (uint8_t)CAZ_PROGRAM_RETURN_TO_CHARGE;
}

static void restore_assigned_program_identity(int droid_index, CazEnvDroid *droid)
{
    droid->program = assignable_program_for_droid(droid_index);
}

static float clampf(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static uint8_t clamp_u8(int value)
{
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return (uint8_t)value;
}

static uint8_t normalized_distance(float distance, float maximum)
{
    if (maximum <= 0.0f) {
        return 255u;
    }
    return clamp_u8((int)(distance / maximum * 255.0f));
}

static float daylight_at(float elapsed_seconds)
{
    return 0.35f + 0.65f * fmaxf(0.0f, sinf(elapsed_seconds * 6.2831852f / 1800.0f));
}

static float wrap_pi(float angle)
{
    while (angle > 3.1415926f) {
        angle -= 6.2831852f;
    }
    while (angle < -3.1415926f) {
        angle += 6.2831852f;
    }
    return angle;
}

static uint32_t next_random(CazEnvState *state)
{
    state->rng = state->rng * 1664525u + 1013904223u;
    return state->rng;
}

static float random01(CazEnvState *state)
{
    return (float)((next_random(state) >> 8) & 0x00ffffffu) / 16777215.0f;
}

static float random_range(CazEnvState *state, float minimum, float maximum)
{
    return minimum + random01(state) * (maximum - minimum);
}

static float distance2(float ax, float az, float bx, float bz)
{
    const float dx = ax - bx;
    const float dz = az - bz;
    return dx * dx + dz * dz;
}

static float charger_x(const CazEnvState *state)
{
    return state->fixtures[0].x;
}

static float charger_z(const CazEnvState *state)
{
    return state->fixtures[0].z;
}

static int is_junction(const CazEnvFixture *fixture)
{
    return fixture->type == CAZ_ENV_FIXTURE_JUNCTION_BOX;
}

static int nearest_junction_index(const CazEnvState *state, float x, float z)
{
    int nearest = -1;
    float nearest_distance = 1000000.0f;
    for (int index = 0; index < CAZ_ENV_FIXTURE_COUNT; index++) {
        const CazEnvFixture *fixture = &state->fixtures[index];
        if (!is_junction(fixture)) {
            continue;
        }
        const float candidate_distance = distance2(x, z, fixture->x, fixture->z);
        if (candidate_distance < nearest_distance) {
            nearest_distance = candidate_distance;
            nearest = index;
        }
    }
    return nearest;
}

static uint8_t available_charge_slots(const CazEnvState *state)
{
    uint8_t count = 0u;
    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        if (state->charge_slots[slot] < 0) {
            count++;
        }
    }
    return count;
}

static uint8_t bearing_to_target(const CazEnvDroid *droid, float target_x, float target_z)
{
    const float dx = target_x - droid->x;
    const float dz = target_z - droid->z;
    const float target_yaw = atan2f(dx, dz);
    const float delta = wrap_pi(target_yaw - droid->yaw);
    return clamp_u8((int)(128.0f + delta * 127.0f / 3.1415926f));
}

static uint8_t distance_to_target(const CazEnvDroid *droid, float target_x, float target_z)
{
    const float distance = sqrtf(distance2(droid->x, droid->z, target_x, target_z));
    return normalized_distance(distance, CAZ_ENV_ROOM_LENGTH_FT);
}

static uint32_t sensor_hash(const CazEnvState *state, int droid_index, uint32_t salt)
{
    uint32_t value = state->rng ^
                     (uint32_t)(droid_index * 0x9e3779b9u) ^
                     (uint32_t)(state->elapsed_seconds * 4.0f) ^
                     salt;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static float fixture_radius(const CazEnvFixture *fixture)
{
    return (fixture->width + fixture->length) * 0.25f;
}

static float wall_clearance(const CazEnvDroid *droid)
{
    const float x_clearance = CAZ_ENV_ROOM_WIDTH_FT * 0.5f - fabsf(droid->x);
    const float z_clearance = CAZ_ENV_ROOM_LENGTH_FT * 0.5f - fabsf(droid->z);
    return fminf(x_clearance, z_clearance);
}

static float nearest_fixture_clearance(const CazEnvState *state,
                                       const CazEnvDroid *droid,
                                       float *out_x,
                                       float *out_z,
                                       uint8_t *out_type)
{
    float nearest = 1000000.0f;
    for (int index = 0; index < CAZ_ENV_FIXTURE_COUNT; index++) {
        const CazEnvFixture *fixture = &state->fixtures[index];
        const float clearance = sqrtf(distance2(droid->x, droid->z, fixture->x, fixture->z)) -
                                fixture_radius(fixture) -
                                0.55f;
        if (clearance < nearest) {
            nearest = clearance;
            if (out_x != NULL) {
                *out_x = fixture->x;
            }
            if (out_z != NULL) {
                *out_z = fixture->z;
            }
            if (out_type != NULL) {
                *out_type = fixture->type;
            }
        }
    }
    return nearest;
}

static float nearest_droid_clearance(const CazEnvState *state,
                                     int droid_index,
                                     float *out_x,
                                     float *out_z,
                                     float *out_speed)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    float nearest = 1000000.0f;
    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        if (index == droid_index) {
            continue;
        }
        const CazEnvDroid *other = &state->droids[index];
        const float clearance = sqrtf(distance2(droid->x, droid->z, other->x, other->z)) - 1.1f;
        if (clearance < nearest) {
            nearest = clearance;
            if (out_x != NULL) {
                *out_x = other->x;
            }
            if (out_z != NULL) {
                *out_z = other->z;
            }
            if (out_speed != NULL) {
                *out_speed = other->speed;
            }
        }
    }
    return nearest;
}

static float proximity_from_clearance(float clearance, float range)
{
    if (clearance <= 0.0f) {
        return 1.0f;
    }
    if (clearance >= range || range <= 0.0f) {
        return 0.0f;
    }
    return 1.0f - clearance / range;
}

static int fixture_allows_overlap(const CazEnvDroid *droid, const CazEnvFixture *fixture)
{
    if (fixture->type == CAZ_ENV_FIXTURE_CHARGER) {
        return droid->mode == CAZ_ENV_DROID_RETURN_TO_CHARGE ||
               droid->mode == CAZ_ENV_DROID_CHARGING;
    }
    if (fixture->type == CAZ_ENV_FIXTURE_JUNCTION_BOX) {
        return droid->mode == CAZ_ENV_DROID_TAP_JUNCTION;
    }
    return 0;
}

static int movement_obstacle_at(const CazEnvState *state,
                                int droid_index,
                                float x,
                                float z,
                                uint8_t *out_obstacle)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    if (fabsf(x) > CAZ_ENV_ROOM_WIDTH_FT * 0.48f ||
        fabsf(z) > CAZ_ENV_ROOM_LENGTH_FT * 0.48f) {
        *out_obstacle = 10u;
        return 1;
    }

    for (int index = 0; index < CAZ_ENV_FIXTURE_COUNT; index++) {
        const CazEnvFixture *fixture = &state->fixtures[index];
        const float radius = fixture_radius(fixture) + 0.45f;
        if (fixture_allows_overlap(droid, fixture)) {
            continue;
        }
        if (distance2(x, z, fixture->x, fixture->z) < radius * radius) {
            *out_obstacle = fixture->type;
            return 1;
        }
    }

    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        if (index == droid_index) {
            continue;
        }
        if (distance2(x, z, state->droids[index].x, state->droids[index].z) < 1.1f * 1.1f) {
            *out_obstacle = 20u;
            return 1;
        }
    }
    *out_obstacle = 0u;
    return 0;
}

static uint8_t terrain_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    const float wall = proximity_from_clearance(wall_clearance(droid), 2.4f);
    const float fixture = proximity_from_clearance(nearest_fixture_clearance(state, droid, NULL, NULL, NULL), 2.0f);
    const float other = proximity_from_clearance(nearest_droid_clearance(state, droid_index, NULL, NULL, NULL), 1.6f);
    const float motion = clampf(droid->speed / 2.6f, 0.0f, 1.0f) * 0.35f;
    const float blocked = droid->obstacle_state != 0u ? 1.0f : 0.0f;
    const float caution = fmaxf(blocked, fmaxf(wall, fmaxf(fixture, other))) + motion;
    if (caution >= 1.05f) {
        return 5u;
    }
    if (caution >= 0.82f) {
        return 4u;
    }
    if (caution >= 0.58f) {
        return 3u;
    }
    if (caution >= 0.34f) {
        return 2u;
    }
    if (caution >= 0.12f) {
        return 1u;
    }
    return 0u;
}

static uint8_t eye_edge_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    const float wall = proximity_from_clearance(wall_clearance(droid), 4.0f);
    const float fixture = proximity_from_clearance(nearest_fixture_clearance(state, droid, NULL, NULL, NULL), 4.5f);
    const float other = proximity_from_clearance(nearest_droid_clearance(state, droid_index, NULL, NULL, NULL), 4.0f);
    const float blocked = droid->obstacle_state != 0u ? 1.0f : 0.0f;
    const float proximity = fmaxf(blocked, fmaxf(wall, fmaxf(fixture, other)));
    return clamp_u8(28 + (int)(proximity * 210.0f) + (int)(sensor_hash(state, droid_index, 0x1201u) & 15u));
}

static uint8_t eye_motion_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    float other_speed = 0.0f;
    const float other_clearance = nearest_droid_clearance(state, droid_index, NULL, NULL, &other_speed);
    const float other_signal = proximity_from_clearance(other_clearance, 7.0f) * (80.0f + other_speed * 48.0f);
    const float own_signal = clampf(droid->speed / 2.6f, 0.0f, 1.0f) * 64.0f;
    return clamp_u8(10 + (int)own_signal + (int)other_signal + (int)(sensor_hash(state, droid_index, 0x1101u) & 23u));
}

static uint8_t eye_luma_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    const float daylight = daylight_at(state->elapsed_seconds);
    uint8_t fixture_type = 0u;
    const float fixture_clearance = nearest_fixture_clearance(state, droid, NULL, NULL, &fixture_type);
    float shade = 0.0f;
    if (fixture_type == CAZ_ENV_FIXTURE_TREE_TALL ||
        fixture_type == CAZ_ENV_FIXTURE_TREE_MID ||
        fixture_type == CAZ_ENV_FIXTURE_TREE_COMPACT) {
        shade = proximity_from_clearance(fixture_clearance, 3.0f) * 0.28f;
    } else if (fixture_type == CAZ_ENV_FIXTURE_CAT_BED) {
        shade = proximity_from_clearance(fixture_clearance, 1.6f) * 0.12f;
    }
    return clamp_u8((int)((daylight - shade) * 255.0f));
}

static uint8_t ear_pattern_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    uint8_t fixture_type = 0u;
    const float fixture_clearance = nearest_fixture_clearance(state, droid, NULL, NULL, &fixture_type);
    float other_speed = 0.0f;
    const float other_clearance = nearest_droid_clearance(state, droid_index, NULL, NULL, &other_speed);
    const uint32_t beat = sensor_hash(state, droid_index, 0x2301u) % 97u;

    if ((fixture_type == CAZ_ENV_FIXTURE_CHARGER || fixture_type == CAZ_ENV_FIXTURE_JUNCTION_BOX) &&
        fixture_clearance < 2.4f) {
        return 4u;
    }
    if (other_clearance < 3.2f && other_speed > 1.2f) {
        return 1u;
    }
    if (other_clearance < 2.5f) {
        return 2u;
    }
    if (beat < 5u && wall_clearance(droid) < 3.5f) {
        return 3u;
    }
    if (beat >= 5u && beat < 9u) {
        return 1u;
    }
    return 0u;
}

static void ear_source_for_droid(const CazEnvState *state,
                                 int droid_index,
                                 uint8_t pattern,
                                 float *out_x,
                                 float *out_z)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    *out_x = droid->x;
    *out_z = droid->z + 1.0f;

    if (pattern == 1u || pattern == 2u) {
        nearest_droid_clearance(state, droid_index, out_x, out_z, NULL);
    } else if (pattern == 4u) {
        const int junction = nearest_junction_index(state, droid->x, droid->z);
        const float charger_distance = distance2(droid->x, droid->z, charger_x(state), charger_z(state));
        const float junction_distance = junction >= 0
                                      ? distance2(droid->x, droid->z, state->fixtures[junction].x, state->fixtures[junction].z)
                                      : 1000000.0f;
        if (junction >= 0 && junction_distance < charger_distance) {
            *out_x = state->fixtures[junction].x;
            *out_z = state->fixtures[junction].z;
        } else {
            *out_x = charger_x(state);
            *out_z = charger_z(state);
        }
    }
}

static uint8_t ear_volume_for_droid(const CazEnvState *state, int droid_index)
{
    const uint8_t pattern = ear_pattern_for_droid(state, droid_index);
    float other_speed = 0.0f;
    const float other_clearance = nearest_droid_clearance(state, droid_index, NULL, NULL, &other_speed);
    int volume = 18 + (int)(sensor_hash(state, droid_index, 0x2001u) & 31u);
    volume += (int)(proximity_from_clearance(other_clearance, 6.0f) * (50.0f + other_speed * 28.0f));
    if (pattern == 1u) {
        volume += 42;
    } else if (pattern == 2u) {
        volume += 56;
    } else if (pattern == 3u) {
        volume += 72;
    } else if (pattern == 4u) {
        volume += 116;
    }
    return clamp_u8(volume);
}

static uint8_t ear_pitch_for_pattern(uint8_t pattern, const CazEnvState *state, int droid_index)
{
    const int noise = (int)(sensor_hash(state, droid_index, 0x2101u) & 31u);
    switch (pattern) {
    case 1u:
        return clamp_u8(176 + noise);
    case 2u:
        return clamp_u8(108 + noise);
    case 3u:
        return clamp_u8(76 + noise);
    case 4u:
        return clamp_u8(42 + noise);
    default:
        return clamp_u8(96 + noise);
    }
}

static uint8_t lifted_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    uint8_t fixture_type = 0u;
    const float fixture_clearance = nearest_fixture_clearance(state, droid, NULL, NULL, &fixture_type);
    if ((fixture_type == CAZ_ENV_FIXTURE_TREE_TALL ||
         fixture_type == CAZ_ENV_FIXTURE_TREE_MID ||
         fixture_type == CAZ_ENV_FIXTURE_TREE_COMPACT) &&
        fixture_clearance < 0.35f &&
        (sensor_hash(state, droid_index, 0x3201u) & 31u) == 0u) {
        return 1u;
    }
    return 0u;
}

static uint8_t dropped_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    if (terrain_for_droid(state, droid_index) >= 5u &&
        droid->speed > 1.9f &&
        (sensor_hash(state, droid_index, 0x3301u) & 63u) == 0u) {
        return 1u;
    }
    return 0u;
}

static uint8_t imu_roll_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    const float sway = sinf(droid->phase) * (4.0f + droid->speed * 4.0f);
    const int terrain = (int)terrain_for_droid(state, droid_index);
    return clamp_u8(128 + (int)sway + terrain * 3 - 6);
}

static uint8_t imu_pitch_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    const float bob = cosf(droid->phase * 0.7f) * (3.0f + droid->speed * 3.0f);
    const int mode_offset = droid->mode == CAZ_ENV_DROID_CHARGING ? -4 :
                            droid->mode == CAZ_ENV_DROID_TAP_JUNCTION ? 8 : 0;
    return clamp_u8(128 + (int)bob + mode_offset);
}

static uint8_t reflex_for_droid(const CazEnvState *state, int droid_index, const CazEnvBytecodeRuntime *runtime)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    const int recovery_motion = runtime->output.nav_intent == CAZ_NAV_CHARGER ||
                                runtime->output.nav_intent == CAZ_NAV_JUNCTION;
    if (droid->charge < 0.13f && runtime->output.gait != 0u && !recovery_motion) {
        return CAZ_BODY_REFLEX_LOW_BATTERY;
    }
    if (dropped_for_droid(state, droid_index) != 0u) {
        return CAZ_BODY_REFLEX_DROPPED;
    }
    if (lifted_for_droid(state, droid_index) != 0u) {
        return CAZ_BODY_REFLEX_LIFTED;
    }
    if (abs((int)imu_roll_for_droid(state, droid_index) - 128) > 18 ||
        abs((int)imu_pitch_for_droid(state, droid_index) - 128) > 18) {
        return CAZ_BODY_REFLEX_BALANCE;
    }
    if (terrain_for_droid(state, droid_index) >= 4u &&
        (runtime->output.gait == 3u || runtime->output.gait == 4u)) {
        return CAZ_BODY_REFLEX_TERRAIN_CAUTION;
    }
    return CAZ_BODY_REFLEX_CLEAR;
}

static void assign_random_target(CazEnvState *state, CazEnvDroid *droid)
{
    droid->target_x = random_range(state, -CAZ_ENV_ROOM_WIDTH_FT * 0.43f, CAZ_ENV_ROOM_WIDTH_FT * 0.43f);
    droid->target_z = random_range(state, -CAZ_ENV_ROOM_LENGTH_FT * 0.43f, CAZ_ENV_ROOM_LENGTH_FT * 0.43f);
}

static void place_fixture(CazEnvState *state,
                          int index,
                          uint8_t type,
                          float width,
                          float length,
                          float height,
                          float yaw)
{
    CazEnvFixture *fixture = &state->fixtures[index];
    fixture->type = type;
    fixture->width = width;
    fixture->length = length;
    fixture->height = height;
    fixture->yaw = yaw;
    fixture->y = height * 0.5f;
    fixture->x = random_range(state,
                              -CAZ_ENV_ROOM_WIDTH_FT * 0.5f + width,
                              CAZ_ENV_ROOM_WIDTH_FT * 0.5f - width);
    fixture->z = random_range(state,
                              -CAZ_ENV_ROOM_LENGTH_FT * 0.5f + length,
                              CAZ_ENV_ROOM_LENGTH_FT * 0.5f - length);
}

static void place_fixture_clear(CazEnvState *state,
                                int index,
                                uint8_t type,
                                float width,
                                float length,
                                float height)
{
    const float yaw = random_range(state, -3.1415926f, 3.1415926f);
    for (int attempt = 0; attempt < 64; attempt++) {
        int clear = 1;
        place_fixture(state, index, type, width, length, height, yaw);
        for (int other = 0; other < index; other++) {
            const CazEnvFixture *a = &state->fixtures[index];
            const CazEnvFixture *b = &state->fixtures[other];
            const float radius = (a->width + a->length + b->width + b->length) * 0.28f;
            if (distance2(a->x, a->z, b->x, b->z) < radius * radius) {
                clear = 0;
                break;
            }
        }
        if (clear) {
            return;
        }
    }
}

static void place_wall_junction(CazEnvState *state, int index, float center_y)
{
    CazEnvFixture *fixture = &state->fixtures[index];
    const int wall = (int)(next_random(state) % 4u);
    fixture->type = CAZ_ENV_FIXTURE_JUNCTION_BOX;
    fixture->width = 1.0f;
    fixture->length = 1.0f;
    fixture->height = 1.0f;
    fixture->y = center_y;

    switch (wall) {
    case 0:
        fixture->x = -CAZ_ENV_ROOM_WIDTH_FT * 0.5f + 0.50f;
        fixture->z = random_range(state, -CAZ_ENV_ROOM_LENGTH_FT * 0.42f, CAZ_ENV_ROOM_LENGTH_FT * 0.42f);
        fixture->yaw = 1.5707963f;
        break;
    case 1:
        fixture->x = CAZ_ENV_ROOM_WIDTH_FT * 0.5f - 0.50f;
        fixture->z = random_range(state, -CAZ_ENV_ROOM_LENGTH_FT * 0.42f, CAZ_ENV_ROOM_LENGTH_FT * 0.42f);
        fixture->yaw = -1.5707963f;
        break;
    case 2:
        fixture->x = random_range(state, -CAZ_ENV_ROOM_WIDTH_FT * 0.42f, CAZ_ENV_ROOM_WIDTH_FT * 0.42f);
        fixture->z = -CAZ_ENV_ROOM_LENGTH_FT * 0.5f + 0.50f;
        fixture->yaw = 0.0f;
        break;
    default:
        fixture->x = random_range(state, -CAZ_ENV_ROOM_WIDTH_FT * 0.42f, CAZ_ENV_ROOM_WIDTH_FT * 0.42f);
        fixture->z = CAZ_ENV_ROOM_LENGTH_FT * 0.5f - 0.50f;
        fixture->yaw = 3.1415926f;
        break;
    }
}

static void occupy_charge_slot(CazEnvState *state, int droid_index)
{
    CazEnvDroid *droid = &state->droids[droid_index];
    if (droid->charging_slot < CAZ_ENV_CHARGE_SLOT_COUNT &&
        state->charge_slots[droid->charging_slot] == droid_index) {
        return;
    }

    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        if (state->charge_slots[slot] < 0) {
            state->charge_slots[slot] = (int8_t)droid_index;
            droid->charging_slot = (uint8_t)slot;
            return;
        }
    }

    droid->charging_slot = 255u;
}

static void release_charge_slot(CazEnvState *state, int droid_index)
{
    CazEnvDroid *droid = &state->droids[droid_index];
    if (droid->charging_slot < CAZ_ENV_CHARGE_SLOT_COUNT &&
        state->charge_slots[droid->charging_slot] == droid_index) {
        state->charge_slots[droid->charging_slot] = -1;
    }
    droid->charging_slot = 255u;
}

static void record_nav_state(CazEnvDroid *droid, uint8_t intent, uint8_t status, uint8_t cause)
{
    droid->nav_status = status;
    droid->nav_cause = cause;
    if (droid->last_nav_intent != intent ||
        droid->last_nav_status != status ||
        droid->last_nav_cause != cause) {
        droid->nav_transition_count++;
        droid->last_nav_intent = intent;
        droid->last_nav_status = status;
        droid->last_nav_cause = cause;
    }
}

static float output_speed_for(uint8_t gait, uint8_t skill, uint8_t reflex_state)
{
    if (reflex_state == CAZ_BODY_REFLEX_LOW_BATTERY ||
        reflex_state == CAZ_BODY_REFLEX_DROPPED ||
        reflex_state == CAZ_BODY_REFLEX_LIFTED) {
        return 0.0f;
    }

    switch (skill) {
    case CAZ_BODY_SKILL_REST:
    case CAZ_BODY_SKILL_SIT:
    case CAZ_BODY_SKILL_GROOM:
    case CAZ_BODY_SKILL_STRETCH:
    case CAZ_BODY_SKILL_RECOVER:
    case CAZ_BODY_SKILL_BALANCE:
        return 0.0f;
    case CAZ_BODY_SKILL_CRAWL:
        return 0.55f;
    case CAZ_BODY_SKILL_POUNCE:
        return 2.85f;
    case CAZ_BODY_SKILL_SCRATCH:
        return 0.42f;
    case CAZ_BODY_SKILL_WALK:
    case CAZ_BODY_SKILL_SNIFF:
        break;
    default:
        break;
    }

    switch (gait % 6u) {
    case 0:
        return 0.0f;
    case 1:
        return 1.55f;
    case 2:
        return 0.62f;
    case 3:
        return 2.75f;
    case 4:
        return 1.25f;
    case 5:
        return 0.48f;
    default:
        return 0.0f;
    }
}

static float yaw_from_output(const CazEnvDroid *droid, uint8_t gait, uint8_t head_yaw)
{
    const float offset = ((float)head_yaw - 128.0f) * (1.5707963f / 127.0f);
    const float retreat = (gait % 6u) == 4u ? 3.1415926f : 0.0f;
    return wrap_pi(droid->yaw + offset + retreat);
}

static void set_target_from_output(CazEnvDroid *droid, uint8_t gait, uint8_t head_yaw)
{
    const float yaw = yaw_from_output(droid, gait, head_yaw);
    const float lookahead = (gait % 6u) == 3u ? 7.0f : 4.5f;
    droid->target_x = clampf(droid->x + sinf(yaw) * lookahead,
                             -CAZ_ENV_ROOM_WIDTH_FT * 0.48f,
                             CAZ_ENV_ROOM_WIDTH_FT * 0.48f);
    droid->target_z = clampf(droid->z + cosf(yaw) * lookahead,
                             -CAZ_ENV_ROOM_LENGTH_FT * 0.48f,
                             CAZ_ENV_ROOM_LENGTH_FT * 0.48f);
}

static void set_charger_loiter_target(const CazEnvState *state, int droid_index, CazEnvDroid *droid)
{
    const float angle = (float)(droid_index % CAZ_ENV_DROID_COUNT) * 2.3999632f;
    const float radius = 2.8f + (float)(droid_index % 3) * 0.45f;
    droid->target_x = clampf(charger_x(state) + sinf(angle) * radius,
                             -CAZ_ENV_ROOM_WIDTH_FT * 0.48f,
                             CAZ_ENV_ROOM_WIDTH_FT * 0.48f);
    droid->target_z = clampf(charger_z(state) + cosf(angle) * radius,
                             -CAZ_ENV_ROOM_LENGTH_FT * 0.48f,
                             CAZ_ENV_ROOM_LENGTH_FT * 0.48f);
}

static void apply_bytecode_motion_outputs(CazEnvState *state, int droid_index)
{
    CazEnvDroid *droid = &state->droids[droid_index];
    CazEnvBytecodeRuntime *runtime = &droid->bytecode;
    const uint8_t reflex = reflex_for_droid(state, droid_index, runtime);
    droid->gait = runtime->output.gait;
    droid->speed = output_speed_for(runtime->output.gait, runtime->output.skill, reflex);
}

static uint8_t read_env_port(const CazEnvBytecodeRuntime *runtime, uint8_t port)
{
    if (runtime == NULL ||
        runtime->state == NULL ||
        runtime->droid_index >= CAZ_ENV_DROID_COUNT) {
        return 0xffu;
    }

    const CazEnvState *state = runtime->state;
    const CazEnvDroid *droid = &state->droids[runtime->droid_index];
    const int droid_index = runtime->droid_index;
    const int junction_index = nearest_junction_index(state, droid->x, droid->z);
    const float daylight = daylight_at(state->elapsed_seconds);
    const uint8_t ear_pattern = ear_pattern_for_droid(state, droid_index);
    float ear_x = droid->x;
    float ear_z = droid->z;
    ear_source_for_droid(state, droid_index, ear_pattern, &ear_x, &ear_z);

    switch (port) {
    case CAZ_PORT_EYE_LUMA:
        return eye_luma_for_droid(state, droid_index);
    case CAZ_PORT_EYE_MOTION:
        return eye_motion_for_droid(state, droid_index);
    case CAZ_PORT_EYE_EDGE:
        return eye_edge_for_droid(state, droid_index);
    case CAZ_PORT_EYE_COLOUR_TEMP:
        return clamp_u8(96 + (int)(daylight * 92.0f));
    case CAZ_PORT_EAR_VOLUME:
        return ear_volume_for_droid(state, droid_index);
    case CAZ_PORT_EAR_PITCH:
        return ear_pitch_for_pattern(ear_pattern, state, droid_index);
    case CAZ_PORT_EAR_BEARING:
        return bearing_to_target(droid, ear_x, ear_z);
    case CAZ_PORT_EAR_PATTERN:
        return ear_pattern;
    case CAZ_PORT_IMU_ROLL:
        return imu_roll_for_droid(state, droid_index);
    case CAZ_PORT_IMU_PITCH:
        return imu_pitch_for_droid(state, droid_index);
    case CAZ_PORT_LIFTED:
        return lifted_for_droid(state, droid_index);
    case CAZ_PORT_DROPPED:
        return dropped_for_droid(state, droid_index);
    case CAZ_PORT_BATTERY:
        return clamp_u8((int)(droid->charge * 255.0f));
    case CAZ_PORT_TERRAIN:
        return terrain_for_droid(state, droid_index);
    case CAZ_PORT_ENERGY_SOURCE:
        return droid->energy_source;
    case CAZ_PORT_CHARGER_BEARING:
        return bearing_to_target(droid, charger_x(state), charger_z(state));
    case CAZ_PORT_CHARGER_DISTANCE:
        return distance_to_target(droid, charger_x(state), charger_z(state));
    case CAZ_PORT_CHARGER_SLOTS:
        return available_charge_slots(state);
    case CAZ_PORT_JUNCTION_BEARING:
        return junction_index >= 0
             ? bearing_to_target(droid, state->fixtures[junction_index].x, state->fixtures[junction_index].z)
             : 128u;
    case CAZ_PORT_JUNCTION_DISTANCE:
        return junction_index >= 0
             ? distance_to_target(droid, state->fixtures[junction_index].x, state->fixtures[junction_index].z)
             : 255u;
    case CAZ_PORT_SOLAR_LEVEL:
        return clamp_u8((int)(daylight * 255.0f));
    case CAZ_PORT_STRATEGY_TENDENCY:
        return clamp_u8((int)(droid->feral * 255.0f));
    case CAZ_PORT_GAIT:
        return runtime->output.gait;
    case CAZ_PORT_HEAD_YAW:
        return runtime->output.head_yaw;
    case CAZ_PORT_EAR_POSE:
        return runtime->output.ear_pose;
    case CAZ_PORT_TAIL_POSE:
        return runtime->output.tail_pose;
    case CAZ_PORT_VOCAL:
        return runtime->output.vocal;
    case CAZ_PORT_EYELID:
        return runtime->output.eyelid;
    case CAZ_PORT_SKILL:
        return runtime->output.skill;
    case CAZ_PORT_SKILL_ARG:
        return runtime->output.skill_arg;
    case CAZ_PORT_SKILL_STATUS:
        return reflex_for_droid(state, droid_index, runtime) != CAZ_BODY_REFLEX_CLEAR
             ? CAZ_BODY_SKILL_REFLEX
             : (runtime->output.skill == CAZ_BODY_SKILL_NONE ? CAZ_BODY_SKILL_IDLE : CAZ_BODY_SKILL_READY);
    case CAZ_PORT_REFLEX_STATE:
        return reflex_for_droid(state, droid_index, runtime);
    case CAZ_PORT_NAV_INTENT:
        return runtime->output.nav_intent;
    case CAZ_PORT_NAV_STATUS:
        return droid->nav_status;
    case CAZ_PORT_JOINT_INDEX:
        return runtime->output.selected_joint;
    case CAZ_PORT_JOINT_ANGLE:
        return runtime->output.pending_joint_angle;
    default:
        return 0xffu;
    }
}

static uint8_t bytecode_read_port(void *user, uint8_t port)
{
    return read_env_port((const CazEnvBytecodeRuntime *)user, port);
}

static void bytecode_write_port(void *user, uint8_t port, uint8_t value)
{
    CazEnvBytecodeRuntime *runtime = (CazEnvBytecodeRuntime *)user;
    if (runtime == NULL) {
        return;
    }

    switch (port) {
    case CAZ_PORT_NAV_INTENT:
        runtime->output.nav_intent = (uint8_t)(value % 4u);
        break;
    case CAZ_PORT_SKILL:
        runtime->output.skill = value;
        break;
    case CAZ_PORT_SKILL_ARG:
        runtime->output.skill_arg = value;
        break;
    case CAZ_PORT_GAIT:
        runtime->output.gait = (uint8_t)(value % 6u);
        break;
    case CAZ_PORT_HEAD_YAW:
        runtime->output.head_yaw = value;
        break;
    case CAZ_PORT_EAR_POSE:
        runtime->output.ear_pose = (uint8_t)(value % 5u);
        break;
    case CAZ_PORT_TAIL_POSE:
        runtime->output.tail_pose = (uint8_t)(value % 9u);
        break;
    case CAZ_PORT_VOCAL:
        runtime->output.vocal = (uint8_t)(value % 6u);
        break;
    case CAZ_PORT_EYELID:
        runtime->output.eyelid = value;
        break;
    case CAZ_PORT_JOINT_INDEX:
        runtime->output.selected_joint = (uint8_t)(value % 16u);
        break;
    case CAZ_PORT_JOINT_ANGLE:
        runtime->output.pending_joint_angle = value;
        break;
    default:
        break;
    }
}

static void initialize_bytecode_runtime(CazEnvState *state, int droid_index, CazEnvDroid *droid, uint8_t program)
{
    CazEnvBytecodeRuntime *runtime = &droid->bytecode;
    const CazProgramMetadata *metadata = program_metadata_for(program);
    memset(runtime, 0, sizeof(*runtime));
    runtime->state = state;
    runtime->assigned_program = program;
    runtime->droid_index = (uint8_t)droid_index;
    runtime->output.nav_intent = CAZ_NAV_WANDER;
    runtime->output.gait = metadata->default_gait;
    runtime->output.head_yaw = 128u;
    runtime->output.ear_pose = 1u;
    runtime->output.tail_pose = 2u;
    runtime->output.eyelid = 180u;
    snprintf(runtime->image.name, sizeof(runtime->image.name), "%s", metadata->name);
    snprintf(runtime->image.description,
             sizeof(runtime->image.description),
             "%s",
             "assigned in CazEnv; host must load the bytecode image before stepping");
    caz_cpu_init(&runtime->cpu, bytecode_read_port, bytecode_write_port, runtime);
}

static void step_bytecode_runtime(CazEnvDroid *droid)
{
    CazEnvBytecodeRuntime *runtime = &droid->bytecode;
    if (!runtime->stepping_enabled || !runtime->image_loaded || runtime->faulted) {
        return;
    }

    for (uint8_t count = 0u; count < CAZ_ENV_VM_INSTRUCTIONS_PER_TICK; count++) {
        if (runtime->cpu.halted) {
            return;
        }
        if (caz_cpu_step(&runtime->cpu) < 0) {
            runtime->faulted = 1u;
            return;
        }
    }
}

void caz_env_init(CazEnvState *state, uint32_t seed)
{
    if (state == NULL) {
        return;
    }

    state->rng = seed == 0u ? 0x0ca7e11du : seed;
    state->elapsed_seconds = 0.0f;
    memset(&state->supervisor_metrics, 0, sizeof(state->supervisor_metrics));
    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        state->charge_slots[slot] = -1;
    }

    place_fixture_clear(state, 0, CAZ_ENV_FIXTURE_CHARGER, 3.0f, 3.0f, 1.0f);

    /* Four archetypes based on common real cat furniture dimensions:
       compact condo, mid tree, tall tower, and free-standing scratch post. */
    place_fixture_clear(state, 1, CAZ_ENV_FIXTURE_TREE_TALL, 1.95f, 1.80f, 6.0f);
    place_fixture_clear(state, 2, CAZ_ENV_FIXTURE_TREE_MID, 1.95f, 1.80f, 3.5f);
    place_fixture_clear(state, 3, CAZ_ENV_FIXTURE_TREE_COMPACT, 1.35f, 1.35f, 1.75f);
    place_fixture_clear(state, 4, CAZ_ENV_FIXTURE_SCRATCH_POST, 1.35f, 1.35f, 2.67f);

    for (int index = 5; index < 17; index++) {
        place_fixture_clear(state, index, CAZ_ENV_FIXTURE_CAT_BED, 2.0f, 1.65f, 0.32f);
    }
    place_wall_junction(state, 17, 0.50f);
    place_wall_junction(state, 18, 0.50f);
    place_wall_junction(state, 19, 4.0f);
    place_wall_junction(state, 20, 4.0f);

    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        CazEnvDroid *droid = &state->droids[index];
        droid->x = random_range(state, -CAZ_ENV_ROOM_WIDTH_FT * 0.40f, CAZ_ENV_ROOM_WIDTH_FT * 0.40f);
        droid->z = random_range(state, -CAZ_ENV_ROOM_LENGTH_FT * 0.40f, CAZ_ENV_ROOM_LENGTH_FT * 0.40f);
        droid->yaw = random_range(state, -3.1415926f, 3.1415926f);
        droid->charge = random_range(state, 0.38f, 1.0f);
        droid->feral = random_range(state, 0.12f, 0.92f);
        droid->solar_gain = 0.0f;
        droid->passive_solar_gain = 0.0f;
        droid->nav_solar_gain = 0.0f;
        droid->fallback_solar_gain = 0.0f;
        droid->charger_gain = 0.0f;
        droid->tap_gain = 0.0f;
        droid->nav_tap_gain = 0.0f;
        droid->fallback_tap_gain = 0.0f;
        restore_assigned_program_identity(index, droid);
        droid->gait = program_metadata_for(droid->program)->default_gait;
        droid->mode = CAZ_ENV_DROID_PROGRAM;
        droid->charging_slot = 255u;
        droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
        droid->nav_status = CAZ_NAV_STATUS_IDLE;
        droid->nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
        droid->last_nav_intent = 255u;
        droid->last_nav_status = 255u;
        droid->last_nav_cause = 255u;
        droid->obstacle_state = 0u;
        droid->nav_transition_count = 0u;
        droid->blocked_movement_count = 0u;
        droid->blocked_junction_count = 0u;
        droid->phase = random_range(state, 0.0f, 6.2831852f);
        droid->speed = program_metadata_for(droid->program)->default_speed;
        initialize_bytecode_runtime(state, index, droid, droid->program);
        assign_random_target(state, droid);
    }
}

static int load_program_for_droid(CazEnvState *state,
                                  int index,
                                  CazProgramKind program,
                                  const char *program_dir,
                                  char *error,
                                  size_t error_length)
{
    const char *dir = (program_dir != NULL && program_dir[0] != '\0') ? program_dir : "programs";
    CazEnvDroid *droid;
    CazEnvBytecodeRuntime *runtime;
    const CazProgramMetadata *metadata = caz_loader_program_metadata(program);
    if (error != NULL && error_length > 0u) {
        error[0] = '\0';
    }
    if (state == NULL || index < 0 || index >= CAZ_ENV_DROID_COUNT || metadata == NULL) {
        if (error != NULL && error_length > 0u) {
            snprintf(error, error_length, "CazEnv program loading failed: invalid program assignment");
        }
        return 0;
    }

    droid = &state->droids[index];
    droid->program = (uint8_t)program;
    droid->gait = metadata->default_gait;
    droid->speed = metadata->default_speed;
    initialize_bytecode_runtime(state, index, droid, (uint8_t)program);
    runtime = &droid->bytecode;

    runtime->image_loaded = 0u;
    runtime->faulted = 0u;
    runtime->stepping_enabled = 0u;
    caz_cpu_init(&runtime->cpu, bytecode_read_port, bytecode_write_port, runtime);
    if (!caz_loader_load_named(&runtime->cpu, metadata->kind, dir, &runtime->image)) {
        if (error != NULL && error_length > 0u) {
            snprintf(error,
                     error_length,
                     "CazEnv failed to load droid %d program %s from %s: %s",
                     index,
                     metadata->name,
                     runtime->image.path,
                     runtime->image.error);
        }
        return 0;
    }
    runtime->image_loaded = 1u;
    runtime->stepping_enabled = 1u;
    return 1;
}

int caz_env_load_programs(CazEnvState *state, const char *program_dir, char *error, size_t error_length)
{
    if (state == NULL) {
        if (error != NULL && error_length > 0u) {
            snprintf(error, error_length, "CazEnv program loading failed: state is null");
        }
        return 0;
    }

    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        if (!load_program_for_droid(state,
                                    index,
                                    (CazProgramKind)state->droids[index].bytecode.assigned_program,
                                    program_dir,
                                    error,
                                    error_length)) {
            return 0;
        }
    }
    return 1;
}

int caz_env_assign_program(CazEnvState *state,
                           int index,
                           CazProgramKind program,
                           const char *program_dir,
                           char *error,
                           size_t error_length)
{
    return load_program_for_droid(state, index, program, program_dir, error, error_length);
}

void caz_env_step(CazEnvState *state, float dt_seconds)
{
    if (state == NULL || dt_seconds <= 0.0f) {
        return;
    }

    dt_seconds = clampf(dt_seconds, 0.0f, 0.2f);
    state->elapsed_seconds += dt_seconds;

    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        CazEnvDroid *droid = &state->droids[index];
        const float charger_dx = charger_x(state) - droid->x;
        const float charger_dz = charger_z(state) - droid->z;
        const float charger_distance = sqrtf(charger_dx * charger_dx + charger_dz * charger_dz);
        const float daylight = 0.35f + 0.65f * fmaxf(0.0f, sinf(state->elapsed_seconds * 6.2831852f / 1800.0f));
        const float passive_solar_gain = daylight * dt_seconds / 172800.0f;
        const float requested_solar_gain = daylight * dt_seconds / 7200.0f;

        step_bytecode_runtime(droid);
        apply_bytecode_motion_outputs(state, index);

        if (droid->mode != CAZ_ENV_DROID_CHARGING && droid->charge < 1.0f) {
            droid->charge = clampf(droid->charge + passive_solar_gain, 0.0f, 1.0f);
            droid->solar_gain += passive_solar_gain;
            droid->passive_solar_gain += passive_solar_gain;
        }
        droid->energy_source = CAZ_ENV_ENERGY_BATTERY;

        const uint8_t nav_intent = droid->bytecode.output.nav_intent;
        const int bytecode_recovery_requested = nav_intent == CAZ_NAV_CHARGER ||
                                                nav_intent == CAZ_NAV_SOLAR ||
                                                nav_intent == CAZ_NAV_JUNCTION;

        if (droid->mode != CAZ_ENV_DROID_CHARGING &&
            droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
            !bytecode_recovery_requested) {
            release_charge_slot(state, index);
            droid->mode = CAZ_ENV_DROID_PROGRAM;
        }

        if (bytecode_recovery_requested && droid->mode != CAZ_ENV_DROID_CHARGING) {
            if (nav_intent == CAZ_NAV_CHARGER) {
                droid->mode = CAZ_ENV_DROID_RETURN_TO_CHARGE;
                droid->target_x = charger_x(state);
                droid->target_z = charger_z(state);
                droid->nav_cause = CAZ_ENV_NAV_CAUSE_BYTECODE;
            } else if (nav_intent == CAZ_NAV_SOLAR) {
                droid->mode = CAZ_ENV_DROID_SOLAR_FORAGE;
                droid->target_x = droid->x;
                droid->target_z = droid->z;
                droid->nav_cause = CAZ_ENV_NAV_CAUSE_BYTECODE;
            } else {
                const int junction = nearest_junction_index(state, droid->x, droid->z);
                if (junction >= 0) {
                    droid->mode = CAZ_ENV_DROID_TAP_JUNCTION;
                    droid->target_x = state->fixtures[junction].x;
                    droid->target_z = state->fixtures[junction].z;
                    droid->nav_cause = CAZ_ENV_NAV_CAUSE_BYTECODE;
                } else {
                    droid->mode = CAZ_ENV_DROID_TAP_JUNCTION;
                    droid->target_x = droid->x;
                    droid->target_z = droid->z;
                    droid->speed = 0.0f;
                    droid->nav_cause = CAZ_ENV_NAV_CAUSE_FAILURE;
                    record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, CAZ_ENV_NAV_CAUSE_FAILURE);
                }
            }
        } else if (droid->mode != CAZ_ENV_DROID_CHARGING &&
                   droid->mode != CAZ_ENV_DROID_TAP_JUNCTION &&
                   droid->mode != CAZ_ENV_DROID_DEPLETED) {
            if (droid->charge <= 0.34f && droid->feral > 0.62f) {
                const int junction = nearest_junction_index(state, droid->x, droid->z);
                if (junction >= 0) {
                    droid->mode = CAZ_ENV_DROID_TAP_JUNCTION;
                    droid->program = (uint8_t)CAZ_PROGRAM_RETURN_TO_CHARGE;
                    droid->target_x = state->fixtures[junction].x;
                    droid->target_z = state->fixtures[junction].z;
                    droid->nav_cause = CAZ_ENV_NAV_CAUSE_SUPERVISOR;
                    state->supervisor_metrics.junction_taps++;
                }
            } else if (droid->charge <= 0.28f && droid->feral > 0.42f) {
                droid->mode = CAZ_ENV_DROID_SOLAR_FORAGE;
                droid->program = (uint8_t)CAZ_PROGRAM_RETURN_TO_CHARGE;
                droid->speed = 0.0f;
                droid->gait = 0u;
                droid->target_x = droid->x;
                droid->target_z = droid->z;
                droid->nav_cause = CAZ_ENV_NAV_CAUSE_SUPERVISOR;
                state->supervisor_metrics.solar_forages++;
                state->supervisor_metrics.speed_overrides++;
                state->supervisor_metrics.gait_overrides++;
            } else if (droid->charge <= 0.22f) {
                droid->mode = CAZ_ENV_DROID_RETURN_TO_CHARGE;
                droid->program = (uint8_t)CAZ_PROGRAM_RETURN_TO_CHARGE;
                droid->target_x = charger_x(state);
                droid->target_z = charger_z(state);
                droid->nav_cause = CAZ_ENV_NAV_CAUSE_SUPERVISOR;
                state->supervisor_metrics.charger_returns++;
            }
        }

        if (droid->charge <= 0.0f && droid->mode != CAZ_ENV_DROID_CHARGING) {
            droid->charge = 0.0f;
            droid->mode = CAZ_ENV_DROID_DEPLETED;
            droid->speed = 0.0f;
            droid->gait = 0u;
            record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, CAZ_ENV_NAV_CAUSE_FAILURE);
        }

        if (droid->mode == CAZ_ENV_DROID_CHARGING) {
            const int bytecode_dock = droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                      nav_intent == CAZ_NAV_CHARGER &&
                                      droid->charging_slot < CAZ_ENV_CHARGE_SLOT_COUNT &&
                                      state->charge_slots[droid->charging_slot] == index;
            if (!bytecode_dock) {
                release_charge_slot(state, index);
                droid->mode = CAZ_ENV_DROID_PROGRAM;
                droid->speed = 0.0f;
                droid->gait = 0u;
                droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
                droid->nav_cause = CAZ_ENV_NAV_CAUSE_FAILURE;
                record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, CAZ_ENV_NAV_CAUSE_FAILURE);
                continue;
            }
            const float charger_gain = dt_seconds / 600.0f;
            droid->charge = clampf(droid->charge + charger_gain, 0.0f, 1.0f);
            droid->charger_gain += charger_gain;
            droid->speed = 0.0f;
            droid->gait = 0u;
            droid->energy_source = CAZ_ENV_ENERGY_CHARGER;
            record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_DOCKED, droid->nav_cause);
            if (droid->charge >= 0.98f) {
                release_charge_slot(state, index);
                restore_assigned_program_identity(index, droid);
                droid->mode = CAZ_ENV_DROID_PROGRAM;
                droid->nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
                set_target_from_output(droid, droid->bytecode.output.gait, droid->bytecode.output.head_yaw);
            }
            continue;
        }

        if (droid->mode == CAZ_ENV_DROID_DEPLETED) {
            droid->energy_source = CAZ_ENV_ENERGY_SOLAR;
            record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, CAZ_ENV_NAV_CAUSE_FAILURE);
            continue;
        }

        if (droid->mode == CAZ_ENV_DROID_TAP_JUNCTION) {
            const int junction = nearest_junction_index(state, droid->x, droid->z);
            if (junction >= 0) {
                const CazEnvFixture *fixture = &state->fixtures[junction];
                const float junction_distance = sqrtf(distance2(droid->x, droid->z, fixture->x, fixture->z));
                const int bytecode_tap = droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                         nav_intent == CAZ_NAV_JUNCTION;
                const float tap_reach = bytecode_tap
                                      ? CAZ_ENV_JUNCTION_CLAW_REACH_FT
                                      : CAZ_ENV_JUNCTION_CONTACT_FT;
                droid->target_x = fixture->x;
                droid->target_z = fixture->z;
                if (!bytecode_tap) {
                    state->supervisor_metrics.gait_overrides++;
                    droid->gait = 5u;
                }
                if (junction_distance < tap_reach) {
                    if (!bytecode_tap) {
                        state->supervisor_metrics.speed_overrides++;
                    }
                    droid->speed = 0.0f;
                    if (bytecode_tap) {
                        const float tap_gain = dt_seconds / 900.0f;
                        droid->charge = clampf(droid->charge + tap_gain, 0.0f, 1.0f);
                        droid->tap_gain += tap_gain;
                        droid->nav_tap_gain += tap_gain;
                        droid->energy_source = CAZ_ENV_ENERGY_JUNCTION;
                        record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_TAPPING, droid->nav_cause);
                        if (droid->charge >= 0.74f) {
                            restore_assigned_program_identity(index, droid);
                            droid->mode = CAZ_ENV_DROID_PROGRAM;
                            droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
                            droid->nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
                            set_target_from_output(droid, droid->bytecode.output.gait, droid->bytecode.output.head_yaw);
                        }
                        continue;
                    } else {
                        record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, droid->nav_cause);
                    }
                } else {
                    if (bytecode_tap && droid->speed < 0.42f) {
                        droid->speed = 0.42f;
                        droid->gait = 5u;
                    }
                    if (bytecode_tap && droid->speed <= 0.01f) {
                        droid->blocked_junction_count++;
                    }
                    if (!bytecode_tap && droid->speed < 1.0f) {
                        state->supervisor_metrics.speed_overrides++;
                        droid->speed = 1.0f;
                    }
                    record_nav_state(droid,
                                     nav_intent,
                                     droid->speed > 0.01f ? CAZ_NAV_STATUS_RUNNING : CAZ_NAV_STATUS_BLOCKED,
                                     droid->nav_cause);
                }
            } else {
                droid->speed = 0.0f;
                record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, CAZ_ENV_NAV_CAUSE_FAILURE);
            }
        } else if (droid->mode == CAZ_ENV_DROID_SOLAR_FORAGE) {
            droid->energy_source = CAZ_ENV_ENERGY_SOLAR;
            if (droid->nav_cause != CAZ_ENV_NAV_CAUSE_BYTECODE) {
                state->supervisor_metrics.speed_overrides++;
                state->supervisor_metrics.gait_overrides++;
            }
            droid->gait = 0u;
            droid->speed = 0.0f;
            droid->target_x = droid->x;
            droid->target_z = droid->z;
            if (droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                nav_intent == CAZ_NAV_SOLAR) {
                droid->charge = clampf(droid->charge + requested_solar_gain, 0.0f, 1.0f);
                droid->solar_gain += requested_solar_gain;
                droid->nav_solar_gain += requested_solar_gain;
            }
            record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_SOLAR, droid->nav_cause);
            if (droid->nav_cause != CAZ_ENV_NAV_CAUSE_BYTECODE && droid->charge >= 0.55f) {
                restore_assigned_program_identity(index, droid);
                droid->mode = CAZ_ENV_DROID_PROGRAM;
                droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
                droid->nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
                set_target_from_output(droid, droid->bytecode.output.gait, droid->bytecode.output.head_yaw);
            }
        } else if (droid->mode == CAZ_ENV_DROID_RETURN_TO_CHARGE) {
            const int bytecode_cause = droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE;
            droid->target_x = charger_x(state);
            droid->target_z = charger_z(state);
            if (!bytecode_cause) {
                state->supervisor_metrics.gait_overrides++;
                droid->gait = 1u;
                if (droid->speed < 1.6f) {
                    state->supervisor_metrics.speed_overrides++;
                    droid->speed = 1.6f;
                }
            }

            if (charger_distance < 1.35f) {
                if (bytecode_cause && nav_intent == CAZ_NAV_CHARGER) {
                    occupy_charge_slot(state, index);
                    if (droid->charging_slot < CAZ_ENV_CHARGE_SLOT_COUNT) {
                        droid->mode = CAZ_ENV_DROID_CHARGING;
                        record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_DOCKED, droid->nav_cause);
                        continue;
                    }
                }
                if (!bytecode_cause) {
                    state->supervisor_metrics.charger_loiters++;
                    state->supervisor_metrics.speed_overrides++;
                    state->supervisor_metrics.gait_overrides++;
                }
                const int low_drain_wait = bytecode_cause &&
                                           droid->bytecode.output.skill == CAZ_BODY_SKILL_REST &&
                                           droid->bytecode.output.gait == 0u;
                droid->speed = low_drain_wait ? 0.0f : (bytecode_cause ? 0.35f : 0.0f);
                droid->gait = bytecode_cause ? droid->gait : 0u;
                set_charger_loiter_target(state, index, droid);
                record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, droid->nav_cause);
            } else {
                record_nav_state(droid,
                                 nav_intent,
                                 droid->speed > 0.01f ? CAZ_NAV_STATUS_RUNNING : CAZ_NAV_STATUS_BLOCKED,
                                 droid->nav_cause);
            }
        } else {
            droid->mode = CAZ_ENV_DROID_PROGRAM;
            if (droid->speed > 0.01f) {
                set_target_from_output(droid, droid->bytecode.output.gait, droid->bytecode.output.head_yaw);
                record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_RUNNING, CAZ_ENV_NAV_CAUSE_BYTECODE);
            } else {
                droid->target_x = droid->x;
                droid->target_z = droid->z;
                record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_IDLE, CAZ_ENV_NAV_CAUSE_NONE);
            }
        }

        const float dx = droid->target_x - droid->x;
        const float dz = droid->target_z - droid->z;
        const float distance = sqrtf(dx * dx + dz * dz);
        droid->obstacle_state = 0u;
        if (distance > 0.0001f) {
            const float step = fminf(distance, droid->speed * dt_seconds);
            const float candidate_x = droid->x + dx / distance * step;
            const float candidate_z = droid->z + dz / distance * step;
            uint8_t obstacle = 0u;
            if (movement_obstacle_at(state, index, candidate_x, candidate_z, &obstacle)) {
                droid->obstacle_state = obstacle;
                droid->blocked_movement_count++;
                droid->speed = 0.0f;
                if (droid->nav_status == CAZ_NAV_STATUS_RUNNING) {
                    record_nav_state(droid, nav_intent, CAZ_NAV_STATUS_BLOCKED, droid->nav_cause);
                }
            } else {
                droid->x = candidate_x;
                droid->z = candidate_z;
                droid->yaw = atan2f(dx, dz);
                droid->phase += dt_seconds * (2.0f + droid->speed);
            }
        }

        const float clamped_x = clampf(droid->x, -CAZ_ENV_ROOM_WIDTH_FT * 0.48f, CAZ_ENV_ROOM_WIDTH_FT * 0.48f);
        const float clamped_z = clampf(droid->z, -CAZ_ENV_ROOM_LENGTH_FT * 0.48f, CAZ_ENV_ROOM_LENGTH_FT * 0.48f);
        if (clamped_x != droid->x || clamped_z != droid->z) {
            droid->obstacle_state = 10u;
            droid->blocked_movement_count++;
            droid->speed = 0.0f;
        }
        droid->x = clamped_x;
        droid->z = clamped_z;

        const float motion_drain = 1.0f / 3600.0f;
        const float idle_drain = 1.0f / 14400.0f;
        const float drain_rate = droid->mode == CAZ_ENV_DROID_SOLAR_FORAGE &&
                                      droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                      nav_intent == CAZ_NAV_SOLAR
                               ? 0.0f
                               : (droid->speed > 0.2f ? motion_drain : idle_drain);
        const float drain = drain_rate * dt_seconds;
        droid->charge = clampf(droid->charge - drain, 0.0f, 1.0f);
        if (droid->charge <= 0.0f && droid->mode != CAZ_ENV_DROID_CHARGING) {
            droid->mode = CAZ_ENV_DROID_DEPLETED;
            droid->speed = 0.0f;
            droid->gait = 0u;
            droid->energy_source = CAZ_ENV_ENERGY_SOLAR;
        }
    }
}

int caz_env_fixture_count(void)
{
    return CAZ_ENV_FIXTURE_COUNT;
}

int caz_env_droid_count(void)
{
    return CAZ_ENV_DROID_COUNT;
}

void caz_env_fixture_snapshot(const CazEnvState *state, int index, CazEnvFixtureSnapshot *out_snapshot)
{
    if (state == NULL || out_snapshot == NULL || index < 0 || index >= CAZ_ENV_FIXTURE_COUNT) {
        return;
    }
    const CazEnvFixture *fixture = &state->fixtures[index];
    out_snapshot->type = fixture->type;
    out_snapshot->x = fixture->x;
    out_snapshot->y = fixture->y;
    out_snapshot->z = fixture->z;
    out_snapshot->width = fixture->width;
    out_snapshot->length = fixture->length;
    out_snapshot->height = fixture->height;
    out_snapshot->yaw = fixture->yaw;
}

void caz_env_droid_snapshot(const CazEnvState *state, int index, CazEnvDroidSnapshot *out_snapshot)
{
    if (state == NULL || out_snapshot == NULL || index < 0 || index >= CAZ_ENV_DROID_COUNT) {
        return;
    }
    const CazEnvDroid *droid = &state->droids[index];
    out_snapshot->x = droid->x;
    out_snapshot->z = droid->z;
    out_snapshot->yaw = droid->yaw;
    out_snapshot->charge = droid->charge;
    out_snapshot->feral = droid->feral;
    out_snapshot->solar_gain = droid->solar_gain;
    out_snapshot->passive_solar_gain = droid->passive_solar_gain;
    out_snapshot->nav_solar_gain = droid->nav_solar_gain;
    out_snapshot->fallback_solar_gain = droid->fallback_solar_gain;
    out_snapshot->charger_gain = droid->charger_gain;
    out_snapshot->tap_gain = droid->tap_gain;
    out_snapshot->nav_tap_gain = droid->nav_tap_gain;
    out_snapshot->fallback_tap_gain = droid->fallback_tap_gain;
    out_snapshot->speed = droid->speed;
    out_snapshot->program = droid->program;
    out_snapshot->gait = droid->gait;
    out_snapshot->mode = droid->mode;
    out_snapshot->charging_slot = droid->charging_slot;
    out_snapshot->energy_source = droid->energy_source;
    out_snapshot->bytecode_loaded = droid->bytecode.image_loaded;
    out_snapshot->bytecode_stepping_enabled = droid->bytecode.stepping_enabled;
    out_snapshot->bytecode_halted = droid->bytecode.cpu.halted ? 1u : 0u;
    out_snapshot->bytecode_faulted = droid->bytecode.faulted;
    out_snapshot->nav_intent = droid->bytecode.output.nav_intent;
    out_snapshot->nav_status = droid->nav_status;
    out_snapshot->nav_cause = droid->nav_cause;
    out_snapshot->skill = droid->bytecode.output.skill;
    out_snapshot->bytecode_gait = droid->bytecode.output.gait;
    out_snapshot->head_yaw = droid->bytecode.output.head_yaw;
    out_snapshot->ear_pose = droid->bytecode.output.ear_pose;
    out_snapshot->tail_pose = droid->bytecode.output.tail_pose;
    out_snapshot->vocal = droid->bytecode.output.vocal;
    out_snapshot->eyelid = droid->bytecode.output.eyelid;
    out_snapshot->reflex_state = reflex_for_droid(state, index, &droid->bytecode);
    out_snapshot->bytecode_program = droid->bytecode.assigned_program;
    out_snapshot->obstacle_state = droid->obstacle_state;
    out_snapshot->nav_transition_count = droid->nav_transition_count;
    out_snapshot->blocked_movement_count = droid->blocked_movement_count;
    out_snapshot->blocked_junction_count = droid->blocked_junction_count;
    out_snapshot->bytecode_instructions = droid->bytecode.cpu.instructions;
    out_snapshot->bytecode_cycles = droid->bytecode.cpu.cycles;
}

const char *caz_env_program_name(uint8_t program)
{
    return program_metadata_for(program)->name;
}

const char *caz_env_bytecode_program_name(const CazEnvState *state, int index)
{
    if (state == NULL || index < 0 || index >= CAZ_ENV_DROID_COUNT) {
        return "unknown";
    }

    const CazEnvBytecodeRuntime *runtime = &state->droids[index].bytecode;
    if (runtime->image.name[0] != '\0') {
        return runtime->image.name;
    }
    return caz_env_program_name(runtime->assigned_program);
}

uint8_t caz_env_debug_read_port(const CazEnvState *state, int index, uint8_t port)
{
    if (state == NULL || index < 0 || index >= CAZ_ENV_DROID_COUNT) {
        return 0xffu;
    }
    return read_env_port(&state->droids[index].bytecode, port);
}
