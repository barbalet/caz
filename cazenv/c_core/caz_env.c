#include "caz_env.h"

#include "../../src/caz_droid.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CazEnvProgramMotion {
    const char *name;
    float speed;
    uint8_t gait;
} CazEnvProgramMotion;

static const CazEnvProgramMotion program_table[CAZ_ENV_PROGRAM_COUNT] = {
    {"curious-patrol", 2.2f, 1u},
    {"nap-watch", 0.35f, 0u},
    {"farmyard-mouser", 1.8f, 2u},
    {"skill-cycle", 1.5f, 1u},
    {"loaf-and-groom", 0.25f, 0u},
    {"stalk-and-pounce", 2.6f, 3u},
    {"farmyard-caution", 1.1f, 4u},
    {"greeting-play", 1.9f, 1u},
    {"pose-frame", 0.75f, 5u},
    {"return-to-charge", 2.4f, 1u}
};

#define CAZ_ENV_VM_INSTRUCTIONS_PER_TICK 1u

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

static uint8_t terrain_for_droid(const CazEnvState *state, int droid_index)
{
    const CazEnvDroid *droid = &state->droids[droid_index];
    const float wall = proximity_from_clearance(wall_clearance(droid), 2.4f);
    const float fixture = proximity_from_clearance(nearest_fixture_clearance(state, droid, NULL, NULL, NULL), 2.0f);
    const float other = proximity_from_clearance(nearest_droid_clearance(state, droid_index, NULL, NULL, NULL), 1.6f);
    const float motion = clampf(droid->speed / 2.6f, 0.0f, 1.0f) * 0.35f;
    const float caution = fmaxf(wall, fmaxf(fixture, other)) + motion;
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
    const float proximity = fmaxf(wall, fmaxf(fixture, other));
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
    if (droid->charge < 0.13f && runtime->output.gait != 0u) {
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

static uint8_t nav_status_for_droid(const CazEnvDroid *droid)
{
    switch (droid->mode) {
    case CAZ_ENV_DROID_RETURN_TO_CHARGE:
        return CAZ_NAV_STATUS_RUNNING;
    case CAZ_ENV_DROID_CHARGING:
        return CAZ_NAV_STATUS_DOCKED;
    case CAZ_ENV_DROID_DEPLETED:
        return CAZ_NAV_STATUS_BLOCKED;
    case CAZ_ENV_DROID_SOLAR_FORAGE:
        return CAZ_NAV_STATUS_SOLAR;
    case CAZ_ENV_DROID_TAP_JUNCTION:
        return droid->speed <= 0.01f ? CAZ_NAV_STATUS_TAPPING : CAZ_NAV_STATUS_RUNNING;
    default:
        return CAZ_NAV_STATUS_IDLE;
    }
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
        return nav_status_for_droid(droid);
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
    memset(runtime, 0, sizeof(*runtime));
    runtime->state = state;
    runtime->assigned_program = program;
    runtime->droid_index = (uint8_t)droid_index;
    runtime->output.nav_intent = CAZ_NAV_WANDER;
    runtime->output.gait = program_table[program].gait;
    runtime->output.head_yaw = 128u;
    runtime->output.ear_pose = 1u;
    runtime->output.tail_pose = 2u;
    runtime->output.eyelid = 180u;
    snprintf(runtime->image.name, sizeof(runtime->image.name), "%s", program_table[program].name);
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
        droid->tap_gain = 0.0f;
        droid->program = (uint8_t)(index % CAZ_ENV_PROGRAM_COUNT);
        droid->gait = program_table[droid->program].gait;
        droid->mode = CAZ_ENV_DROID_PROGRAM;
        droid->charging_slot = 255u;
        droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
        droid->phase = random_range(state, 0.0f, 6.2831852f);
        droid->speed = program_table[droid->program].speed;
        initialize_bytecode_runtime(state, index, droid, droid->program);
        assign_random_target(state, droid);
    }
}

int caz_env_load_programs(CazEnvState *state, const char *program_dir, char *error, size_t error_length)
{
    const char *dir = (program_dir != NULL && program_dir[0] != '\0') ? program_dir : "programs";
    if (error != NULL && error_length > 0u) {
        error[0] = '\0';
    }
    if (state == NULL) {
        if (error != NULL && error_length > 0u) {
            snprintf(error, error_length, "CazEnv program loading failed: state is null");
        }
        return 0;
    }

    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        CazEnvDroid *droid = &state->droids[index];
        CazEnvBytecodeRuntime *runtime = &droid->bytecode;
        char path[CAZ_PROGRAM_PATH_MAX];
        const char *name = program_table[runtime->assigned_program].name;
        const int written = snprintf(path, sizeof(path), "%s/%s.caz", dir, name);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            if (error != NULL && error_length > 0u) {
                snprintf(error, error_length, "CazEnv program path is too long for droid %d program %s", index, name);
            }
            return 0;
        }

        runtime->image_loaded = 0u;
        runtime->faulted = 0u;
        runtime->stepping_enabled = 0u;
        caz_cpu_init(&runtime->cpu, bytecode_read_port, bytecode_write_port, runtime);
        if (!caz_loader_load_file(&runtime->cpu, path, &runtime->image)) {
            if (error != NULL && error_length > 0u) {
                snprintf(error,
                         error_length,
                         "CazEnv failed to load droid %d program %s from %s: %s",
                         index,
                         name,
                         path,
                         runtime->image.error);
            }
            return 0;
        }
        runtime->image_loaded = 1u;
        runtime->stepping_enabled = 1u;
    }

    return 1;
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
        const float solar_gain = daylight * dt_seconds / 28800.0f;

        step_bytecode_runtime(droid);

        if (droid->mode != CAZ_ENV_DROID_CHARGING && droid->charge < 1.0f) {
            droid->charge = clampf(droid->charge + solar_gain, 0.0f, 1.0f);
            droid->solar_gain += solar_gain;
        }
        droid->energy_source = CAZ_ENV_ENERGY_BATTERY;

        if (droid->mode == CAZ_ENV_DROID_DEPLETED && droid->charge > 0.03f) {
            droid->mode = CAZ_ENV_DROID_SOLAR_FORAGE;
            droid->gait = 0u;
            droid->speed = 0.18f;
            assign_random_target(state, droid);
        }

        if (droid->mode != CAZ_ENV_DROID_CHARGING &&
            droid->mode != CAZ_ENV_DROID_TAP_JUNCTION &&
            droid->mode != CAZ_ENV_DROID_DEPLETED) {
            if (droid->charge <= 0.34f && droid->feral > 0.62f) {
                const int junction = nearest_junction_index(state, droid->x, droid->z);
                if (junction >= 0) {
                    droid->mode = CAZ_ENV_DROID_TAP_JUNCTION;
                    droid->program = 9u;
                    droid->target_x = state->fixtures[junction].x;
                    droid->target_z = state->fixtures[junction].z;
                }
            } else if (droid->charge <= 0.28f && droid->feral > 0.42f) {
                droid->mode = CAZ_ENV_DROID_SOLAR_FORAGE;
                droid->program = 9u;
                droid->speed = 0.0f;
                droid->gait = 0u;
                droid->target_x = droid->x;
                droid->target_z = droid->z;
            } else if (droid->charge <= 0.22f) {
                droid->mode = CAZ_ENV_DROID_RETURN_TO_CHARGE;
                droid->program = 9u;
            }
        }

        if (droid->charge <= 0.0f && droid->mode != CAZ_ENV_DROID_CHARGING) {
            droid->charge = 0.0f;
            droid->mode = CAZ_ENV_DROID_DEPLETED;
            droid->speed = 0.0f;
            droid->gait = 0u;
        }

        if (droid->mode == CAZ_ENV_DROID_CHARGING) {
            droid->charge = clampf(droid->charge + dt_seconds / 600.0f, 0.0f, 1.0f);
            droid->speed = 0.0f;
            droid->gait = 0u;
            droid->energy_source = CAZ_ENV_ENERGY_CHARGER;
            if (droid->charge >= 0.98f) {
                release_charge_slot(state, index);
                droid->program = (uint8_t)(index % (CAZ_ENV_PROGRAM_COUNT - 1));
                droid->mode = CAZ_ENV_DROID_PROGRAM;
                droid->speed = program_table[droid->program].speed;
                droid->gait = program_table[droid->program].gait;
                assign_random_target(state, droid);
            }
            continue;
        }

        if (droid->mode == CAZ_ENV_DROID_DEPLETED) {
            droid->energy_source = CAZ_ENV_ENERGY_SOLAR;
            continue;
        }

        if (droid->mode == CAZ_ENV_DROID_TAP_JUNCTION) {
            const int junction = nearest_junction_index(state, droid->x, droid->z);
            if (junction >= 0) {
                const CazEnvFixture *fixture = &state->fixtures[junction];
                const float junction_distance = sqrtf(distance2(droid->x, droid->z, fixture->x, fixture->z));
                droid->target_x = fixture->x;
                droid->target_z = fixture->z;
                droid->energy_source = CAZ_ENV_ENERGY_JUNCTION;
                droid->gait = 5u;
                if (junction_distance < 0.95f) {
                    const float tap_gain = dt_seconds / 900.0f;
                    droid->charge = clampf(droid->charge + tap_gain, 0.0f, 1.0f);
                    droid->tap_gain += tap_gain;
                    droid->speed = 0.0f;
                    if (droid->charge >= 0.74f) {
                        droid->program = (uint8_t)(index % (CAZ_ENV_PROGRAM_COUNT - 1));
                        droid->mode = CAZ_ENV_DROID_PROGRAM;
                        droid->speed = program_table[droid->program].speed;
                        droid->gait = program_table[droid->program].gait;
                        droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
                        assign_random_target(state, droid);
                    }
                    continue;
                }
                droid->speed = 1.65f;
            }
        } else if (droid->mode == CAZ_ENV_DROID_SOLAR_FORAGE) {
            droid->energy_source = CAZ_ENV_ENERGY_SOLAR;
            droid->gait = 0u;
            if (distance2(droid->x, droid->z, droid->target_x, droid->target_z) < 0.75f) {
                droid->speed = 0.0f;
                droid->charge = clampf(droid->charge + solar_gain * 4.0f, 0.0f, 1.0f);
                droid->solar_gain += solar_gain * 4.0f;
                if (droid->charge >= 0.55f) {
                    droid->program = (uint8_t)(index % (CAZ_ENV_PROGRAM_COUNT - 1));
                    droid->mode = CAZ_ENV_DROID_PROGRAM;
                    droid->speed = program_table[droid->program].speed;
                    droid->gait = program_table[droid->program].gait;
                    droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
                    assign_random_target(state, droid);
                }
            } else {
                droid->speed = 0.32f;
            }
        } else if (droid->mode == CAZ_ENV_DROID_RETURN_TO_CHARGE) {
            droid->target_x = charger_x(state);
            droid->target_z = charger_z(state);
            droid->speed = program_table[9].speed;
            droid->gait = 1u;

            if (charger_distance < 1.35f) {
                occupy_charge_slot(state, index);
                if (droid->charging_slot < CAZ_ENV_CHARGE_SLOT_COUNT) {
                    droid->mode = CAZ_ENV_DROID_CHARGING;
                    continue;
                }
                droid->speed = 0.0f;
                droid->gait = 0u;
                droid->target_x = charger_x(state) + random_range(state, -3.2f, 3.2f);
                droid->target_z = charger_z(state) + random_range(state, -3.2f, 3.2f);
            }
        } else if (distance2(droid->x, droid->z, droid->target_x, droid->target_z) < 0.85f) {
            assign_random_target(state, droid);
            droid->mode = CAZ_ENV_DROID_PROGRAM;
            droid->speed = program_table[droid->program].speed;
            droid->gait = program_table[droid->program].gait;
        }

        const float dx = droid->target_x - droid->x;
        const float dz = droid->target_z - droid->z;
        const float distance = sqrtf(dx * dx + dz * dz);
        if (distance > 0.0001f) {
            const float step = fminf(distance, droid->speed * dt_seconds);
            droid->x += dx / distance * step;
            droid->z += dz / distance * step;
            droid->yaw = atan2f(dx, dz);
            droid->phase += dt_seconds * (2.0f + droid->speed);
        }

        droid->x = clampf(droid->x, -CAZ_ENV_ROOM_WIDTH_FT * 0.48f, CAZ_ENV_ROOM_WIDTH_FT * 0.48f);
        droid->z = clampf(droid->z, -CAZ_ENV_ROOM_LENGTH_FT * 0.48f, CAZ_ENV_ROOM_LENGTH_FT * 0.48f);

        const float motion_drain = 1.0f / 3600.0f;
        const float idle_drain = 1.0f / 14400.0f;
        const float drain_rate = droid->mode == CAZ_ENV_DROID_SOLAR_FORAGE
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
    out_snapshot->tap_gain = droid->tap_gain;
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
    out_snapshot->skill = droid->bytecode.output.skill;
    out_snapshot->bytecode_program = droid->bytecode.assigned_program;
    out_snapshot->bytecode_instructions = droid->bytecode.cpu.instructions;
    out_snapshot->bytecode_cycles = droid->bytecode.cpu.cycles;
}

const char *caz_env_program_name(uint8_t program)
{
    if (program >= CAZ_ENV_PROGRAM_COUNT) {
        return "unknown";
    }
    return program_table[program].name;
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
