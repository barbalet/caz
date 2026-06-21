#include "caz_env.h"

#include "../../src/caz_droid.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
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

static uint8_t bytecode_read_port(void *user, uint8_t port)
{
    (void)user;
    (void)port;
    return 0xffu;
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

static void initialize_bytecode_runtime(CazEnvDroid *droid, uint8_t program)
{
    CazEnvBytecodeRuntime *runtime = &droid->bytecode;
    memset(runtime, 0, sizeof(*runtime));
    runtime->assigned_program = program;
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
             "assigned in CazEnv; bytecode image loading is scheduled for Cycle 15");
    caz_cpu_init(&runtime->cpu, bytecode_read_port, bytecode_write_port, runtime);
}

static void step_bytecode_runtime(CazEnvDroid *droid)
{
    CazEnvBytecodeRuntime *runtime = &droid->bytecode;
    if (!runtime->stepping_enabled || !runtime->image_loaded || runtime->faulted) {
        return;
    }

    if (caz_cpu_step(&runtime->cpu) < 0) {
        runtime->faulted = 1u;
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
        initialize_bytecode_runtime(droid, droid->program);
        assign_random_target(state, droid);
    }
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
