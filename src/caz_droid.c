#include "caz_droid.h"

#include <string.h>

static uint32_t next_random(CazDroid *droid)
{
    droid->rng = droid->rng * 1664525u + 1013904223u;
    return droid->rng;
}

static uint8_t random_u8(CazDroid *droid, uint8_t limit)
{
    if (limit == 0u) {
        return 0u;
    }
    return (uint8_t)((next_random(droid) >> 16) % limit);
}

static uint8_t random_byte(CazDroid *droid)
{
    uint32_t value = next_random(droid);
    value ^= value >> 11;
    value ^= value >> 19;
    return (uint8_t)value;
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

static uint8_t centered_sensor(int offset)
{
    return clamp_u8(128 + offset);
}

static uint8_t terrain_for_scenario(CazScenario scenario)
{
    switch (scenario) {
    case CAZ_SCENARIO_KITCHEN: return 1u;
    case CAZ_SCENARIO_FARMYARD: return 2u;
    case CAZ_SCENARIO_NIGHT_PARLOUR: return 3u;
    case CAZ_SCENARIO_HEDGEROW: return 4u;
    default: return 0u;
    }
}

static int gait_pitch_offset(uint8_t gait)
{
    switch (gait % 6u) {
    case 1: return 4;
    case 2: return -10;
    case 3: return 18;
    case 4: return -6;
    case 5: return 8;
    default: return 0;
    }
}

static uint8_t lifted_sensor(const CazDroid *droid)
{
    if (droid->scenario == CAZ_SCENARIO_KITCHEN &&
        droid->body_ticks != 0u &&
        (droid->body_ticks % 41u) == 0u &&
        droid->ear_pattern == 2u) {
        return 255u;
    }
    return 0u;
}

static uint8_t dropped_sensor(const CazDroid *droid)
{
    if (droid->scenario == CAZ_SCENARIO_HEDGEROW &&
        droid->body_ticks != 0u &&
        (droid->body_ticks % 7u) == 0u &&
        droid->body.requested_gait == 3u) {
        return 255u;
    }
    return 0u;
}

static int triangular_phase(uint64_t tick, int period, int peak)
{
    int phase = (int)(tick % (uint64_t)period);
    int half = period / 2;
    if (phase <= half) {
        return (phase * peak) / half;
    }
    return ((period - phase) * peak) / half;
}

static void set_world(CazDroid *droid,
                      int light,
                      int motion,
                      int edge,
                      int colour_temp,
                      int volume,
                      int pitch,
                      int bearing,
                      int pattern)
{
    droid->eye_luma = clamp_u8(light);
    droid->eye_motion = clamp_u8(motion);
    droid->eye_edge = clamp_u8(edge);
    droid->eye_colour_temp = clamp_u8(colour_temp);
    droid->ear_volume = clamp_u8(volume);
    droid->ear_pitch = clamp_u8(pitch);
    droid->ear_bearing = clamp_u8(bearing);
    droid->ear_pattern = clamp_u8(pattern);
}

static void sync_legacy_pose_fields(CazDroid *droid)
{
    droid->gait = droid->body.gait;
    droid->head_yaw = droid->body.head_yaw;
    droid->ear_pose = droid->body.ear_pose;
    droid->tail_pose = droid->body.tail_pose;
    droid->vocal = droid->body.vocal;
    droid->eyelid = droid->body.eyelid;
}

static void update_body_sensors(CazDroid *droid)
{
    int yaw_offset = ((int)droid->body.requested_head_yaw - 128) / 8;
    int gait_pitch = gait_pitch_offset(droid->body.requested_gait);
    if (droid->body.requested_gait == 3u &&
        (droid->body.requested_head_yaw < 40u || droid->body.requested_head_yaw > 216u)) {
        gait_pitch += 48;
    }
    caz_body_set_normalized_sensors(&droid->body,
                                    centered_sensor(yaw_offset),
                                    centered_sensor(gait_pitch),
                                    lifted_sensor(droid),
                                    dropped_sensor(droid),
                                    clamp_u8(droid->energy),
                                    terrain_for_scenario(droid->scenario));
    caz_body_apply_reflexes(&droid->body);
}

static void update_pose(CazDroid *droid)
{
    int gait = droid->body.gait % 6u;
    int yaw = (int)droid->body.head_yaw - 128;

    if (gait == 1 || gait == 3 || gait == 5) {
        droid->heading += yaw / 32;
        while (droid->heading < 0) {
            droid->heading += 360;
        }
        while (droid->heading >= 360) {
            droid->heading -= 360;
        }
    }

    if (gait == 1) {
        droid->x += (int16_t)((droid->heading > 45 && droid->heading < 135) ? 1 : 0);
        droid->x -= (int16_t)((droid->heading > 225 && droid->heading < 315) ? 1 : 0);
        droid->y += (int16_t)((droid->heading <= 45 || droid->heading >= 315) ? 1 : 0);
        droid->y -= (int16_t)((droid->heading >= 135 && droid->heading <= 225) ? 1 : 0);
        droid->energy -= 1;
        droid->curiosity += 1;
    } else if (gait == 2) {
        droid->energy -= 1;
        droid->comfort += 1;
    } else if (gait == 3) {
        droid->x += (int16_t)((droid->body.head_yaw > 128u) ? 2 : -2);
        droid->energy -= 4;
        droid->curiosity += 3;
    } else if (gait == 4) {
        droid->x -= (int16_t)((droid->body.head_yaw > 128u) ? 1 : -1);
        droid->energy -= 2;
        droid->comfort -= 2;
    } else if (gait == 5) {
        droid->energy -= 1;
        droid->curiosity += 2;
    } else {
        droid->energy += 1;
        droid->comfort += 1;
    }

    if (droid->body.vocal == 3u) {
        droid->comfort += 2;
    } else if (droid->body.vocal == 4u) {
        droid->comfort -= 2;
    }

    if (droid->eye_motion > 150u || droid->ear_volume > 160u) {
        droid->curiosity += 1;
    }

    if (droid->energy < 0) {
        droid->energy = 0;
    } else if (droid->energy > 255) {
        droid->energy = 255;
    }

    if (droid->curiosity < 0) {
        droid->curiosity = 0;
    } else if (droid->curiosity > 255) {
        droid->curiosity = 255;
    }

    if (droid->comfort < 0) {
        droid->comfort = 0;
    } else if (droid->comfort > 255) {
        droid->comfort = 255;
    }
}

void caz_droid_init(CazDroid *droid, CazScenario scenario, uint32_t seed)
{
    memset(droid, 0, sizeof(*droid));
    caz_body_init(&droid->body);
    droid->scenario = scenario;
    droid->rng = seed == 0u ? 0xc0ffee11u : seed;
    sync_legacy_pose_fields(droid);
    droid->energy = 210;
    droid->curiosity = 96;
    droid->comfort = 144;
    droid->heading = 0;
    update_body_sensors(droid);
    sync_legacy_pose_fields(droid);
}

void caz_droid_tick(CazDroid *droid)
{
    droid->body_ticks++;

    int jitter = (int)random_u8(droid, 18u) - 9;
    int motion = 0;
    int light = 0;
    int edge = 0;
    int colour = 128;
    int volume = 0;
    int pitch = 0;
    int bearing = (int)random_byte(droid);
    int pattern = 0;

    switch (droid->scenario) {
    case CAZ_SCENARIO_KITCHEN:
        light = 168 + triangular_phase(droid->body_ticks, 80, 36) + jitter;
        motion = 18 + (int)random_u8(droid, 50u);
        edge = 92 + (int)random_u8(droid, 64u);
        colour = 168;
        volume = 34 + (int)random_u8(droid, 44u);
        pitch = 86 + (int)random_u8(droid, 80u);
        if ((droid->body_ticks % 17u) < 4u) {
            motion += 110;
            edge += 70;
            pattern = 2;
            volume += 56;
        }
        break;
    case CAZ_SCENARIO_FARMYARD:
        light = 130 + triangular_phase(droid->body_ticks, 96, 92) + jitter;
        motion = 28 + (int)random_u8(droid, 72u);
        edge = 66 + (int)random_u8(droid, 88u);
        colour = 142;
        volume = 42 + (int)random_u8(droid, 68u);
        pitch = 68 + (int)random_u8(droid, 150u);
        if ((droid->body_ticks % 29u) < 5u) {
            pattern = 4;
            volume += 110;
            pitch = 44 + (int)random_u8(droid, 44u);
            motion += 24;
        } else if ((droid->body_ticks % 13u) == 0u || random_u8(droid, 100u) < 11u) {
            pattern = 1;
            volume += 38;
            pitch = 176 + (int)random_u8(droid, 54u);
            motion += 120;
            edge += 84;
        } else if ((droid->body_ticks % 37u) < 3u) {
            pattern = 2;
            volume += 42;
        } else {
            pattern = 3;
        }
        break;
    case CAZ_SCENARIO_NIGHT_PARLOUR:
        light = 18 + (int)random_u8(droid, 24u);
        motion = 4 + (int)random_u8(droid, 44u);
        edge = 22 + (int)random_u8(droid, 66u);
        colour = 92;
        volume = 22 + (int)random_u8(droid, 52u);
        pitch = 110 + (int)random_u8(droid, 120u);
        if ((droid->body_ticks % 19u) < 3u) {
            pattern = 1;
            volume += 54;
            motion += 130;
            edge += 112;
        } else if ((droid->body_ticks % 31u) == 0u) {
            pattern = 2;
            volume += 50;
        }
        break;
    case CAZ_SCENARIO_HEDGEROW:
    default:
        light = 84 + triangular_phase(droid->body_ticks, 64, 70) + jitter;
        motion = 40 + (int)random_u8(droid, 100u);
        edge = 48 + (int)random_u8(droid, 110u);
        colour = 118;
        volume = 32 + (int)random_u8(droid, 98u);
        pitch = 90 + (int)random_u8(droid, 140u);
        if ((droid->body_ticks % 11u) == 0u || random_u8(droid, 100u) < 18u) {
            pattern = 1;
            motion += 100;
            edge += 90;
            volume += 42;
        } else if ((droid->body_ticks % 23u) < 5u) {
            pattern = 3;
            volume += 36;
        }
        break;
    }

    set_world(droid, light, motion, edge, colour, volume, pitch, bearing, pattern);
    update_pose(droid);
    update_body_sensors(droid);
    caz_body_tick(&droid->body);
    sync_legacy_pose_fields(droid);
}

uint8_t caz_droid_read_port(void *user, uint8_t port)
{
    CazDroid *droid = (CazDroid *)user;
    uint8_t body_value;
    switch (port) {
    case CAZ_PORT_EYE_LUMA: return droid->eye_luma;
    case CAZ_PORT_EYE_MOTION: return droid->eye_motion;
    case CAZ_PORT_EYE_EDGE: return droid->eye_edge;
    case CAZ_PORT_EYE_COLOUR_TEMP: return droid->eye_colour_temp;
    case CAZ_PORT_EAR_VOLUME: return droid->ear_volume;
    case CAZ_PORT_EAR_PITCH: return droid->ear_pitch;
    case CAZ_PORT_EAR_BEARING: return droid->ear_bearing;
    case CAZ_PORT_EAR_PATTERN: return droid->ear_pattern;
    default:
        return caz_body_read_port(&droid->body, port, &body_value) ? body_value : 0xffu;
    }
}

void caz_droid_write_port(void *user, uint8_t port, uint8_t value)
{
    CazDroid *droid = (CazDroid *)user;
    if (caz_body_write_port(&droid->body, port, value)) {
        update_body_sensors(droid);
        sync_legacy_pose_fields(droid);
    }
}

void caz_droid_print_report(const CazDroid *droid, FILE *out)
{
    fprintf(out,
            "tick=%04llu %-14s eyes{luma=%3u motion=%3u edge=%3u temp=%3u} "
            "ears{vol=%3u pitch=%3u bearing=%3u pattern=%-9s} "
            "pose{gait=%-8s head=%3u ears=%-8s tail=%-9s vocal=%-7s eyelid=%3u} "
            "body{imu=(%3u,%3u) lifted=%3u dropped=%3u battery=%3u terrain=%u skill=%-8s status=%-7s reflex=%-11s joint[%02u:%-14s]=%3u->%3u pose[%02u]=%3u} "
            "state{energy=%3d curiosity=%3d comfort=%3d xy=(%d,%d)}\n",
            (unsigned long long)droid->body_ticks,
            caz_scenario_name(droid->scenario),
            droid->eye_luma,
            droid->eye_motion,
            droid->eye_edge,
            droid->eye_colour_temp,
            droid->ear_volume,
            droid->ear_pitch,
            droid->ear_bearing,
            caz_pattern_name(droid->ear_pattern),
            caz_gait_name(droid->gait),
            droid->head_yaw,
            caz_ear_pose_name(droid->ear_pose),
            caz_tail_pose_name(droid->tail_pose),
            caz_vocal_name(droid->vocal),
            droid->eyelid,
            droid->body.imu_roll,
            droid->body.imu_pitch,
            droid->body.lifted,
            droid->body.dropped,
            droid->body.battery,
            droid->body.terrain,
            caz_body_skill_name(droid->body.active_skill),
            caz_body_skill_status_name(droid->body.skill_status),
            caz_body_reflex_state_name(droid->body.reflex_state),
            droid->body.selected_joint,
            caz_body_joint_name(droid->body.selected_joint),
            droid->body.joint_values[droid->body.selected_joint],
            droid->body.joint_targets[droid->body.selected_joint],
            droid->body.pose_frame_index,
            droid->body.pose_frame[droid->body.pose_frame_index],
            droid->energy,
            droid->curiosity,
            droid->comfort,
            droid->x,
            droid->y);
}

const char *caz_scenario_name(CazScenario scenario)
{
    switch (scenario) {
    case CAZ_SCENARIO_KITCHEN: return "kitchen";
    case CAZ_SCENARIO_FARMYARD: return "farmyard";
    case CAZ_SCENARIO_NIGHT_PARLOUR: return "night-parlour";
    case CAZ_SCENARIO_HEDGEROW: return "hedgerow";
    default: return "unknown";
    }
}

bool caz_scenario_parse(const char *name, CazScenario *scenario)
{
    if (strcmp(name, "kitchen") == 0) {
        *scenario = CAZ_SCENARIO_KITCHEN;
    } else if (strcmp(name, "farmyard") == 0) {
        *scenario = CAZ_SCENARIO_FARMYARD;
    } else if (strcmp(name, "night-parlour") == 0 || strcmp(name, "night") == 0) {
        *scenario = CAZ_SCENARIO_NIGHT_PARLOUR;
    } else if (strcmp(name, "hedgerow") == 0) {
        *scenario = CAZ_SCENARIO_HEDGEROW;
    } else {
        return false;
    }
    return true;
}

void caz_scenario_print_all(FILE *out)
{
    fprintf(out, "kitchen\nfarmyard\nnight-parlour\nhedgerow\n");
}

const char *caz_port_name(uint8_t port)
{
    switch (port) {
    case CAZ_PORT_EYE_LUMA: return "EYE_LUMA";
    case CAZ_PORT_EYE_MOTION: return "EYE_MOTION";
    case CAZ_PORT_EYE_EDGE: return "EYE_EDGE";
    case CAZ_PORT_EYE_COLOUR_TEMP: return "EYE_COLOUR_TEMP";
    case CAZ_PORT_EAR_VOLUME: return "EAR_VOLUME";
    case CAZ_PORT_EAR_PITCH: return "EAR_PITCH";
    case CAZ_PORT_EAR_BEARING: return "EAR_BEARING";
    case CAZ_PORT_EAR_PATTERN: return "EAR_PATTERN";
    case CAZ_PORT_IMU_ROLL: return "IMU_ROLL";
    case CAZ_PORT_IMU_PITCH: return "IMU_PITCH";
    case CAZ_PORT_LIFTED: return "LIFTED";
    case CAZ_PORT_DROPPED: return "DROPPED";
    case CAZ_PORT_BATTERY: return "BATTERY";
    case CAZ_PORT_TERRAIN: return "TERRAIN";
    case CAZ_PORT_GAIT: return "GAIT";
    case CAZ_PORT_HEAD_YAW: return "HEAD_YAW";
    case CAZ_PORT_EAR_POSE: return "EAR_POSE";
    case CAZ_PORT_TAIL_POSE: return "TAIL_POSE";
    case CAZ_PORT_VOCAL: return "VOCAL";
    case CAZ_PORT_EYELID: return "EYELID";
    case CAZ_PORT_SKILL: return "SKILL";
    case CAZ_PORT_SKILL_ARG: return "SKILL_ARG";
    case CAZ_PORT_SKILL_STATUS: return "SKILL_STATUS";
    case CAZ_PORT_REFLEX_STATE: return "REFLEX_STATE";
    case CAZ_PORT_JOINT_INDEX: return "JOINT_INDEX";
    case CAZ_PORT_JOINT_ANGLE: return "JOINT_ANGLE";
    case CAZ_PORT_JOINT_COMMIT: return "JOINT_COMMIT";
    case CAZ_PORT_POSE_FRAME_INDEX: return "POSE_FRAME_INDEX";
    case CAZ_PORT_POSE_FRAME_VALUE: return "POSE_FRAME_VALUE";
    case CAZ_PORT_POSE_FRAME_FLAGS: return "POSE_FRAME_FLAGS";
    case CAZ_PORT_POSE_FRAME_TIME: return "POSE_FRAME_TIME";
    case CAZ_PORT_POSE_FRAME_COMMIT: return "POSE_FRAME_COMMIT";
    default: return "UNKNOWN";
    }
}

const char *caz_pattern_name(uint8_t pattern)
{
    switch (pattern) {
    case 0: return "silence";
    case 1: return "prey";
    case 2: return "human";
    case 3: return "weather";
    case 4: return "machine";
    case 5: return "unknown";
    default: return "unknown";
    }
}

const char *caz_gait_name(uint8_t gait)
{
    switch (gait % 6u) {
    case 0: return "loaf";
    case 1: return "walk";
    case 2: return "crouch";
    case 3: return "pounce";
    case 4: return "retreat";
    case 5: return "paw-test";
    default: return "unknown";
    }
}

const char *caz_ear_pose_name(uint8_t pose)
{
    switch (pose % 5u) {
    case 0: return "neutral";
    case 1: return "scan";
    case 2: return "forward";
    case 3: return "flat";
    case 4: return "swivel";
    default: return "unknown";
    }
}

const char *caz_tail_pose_name(uint8_t pose)
{
    switch (pose % 9u) {
    case 0: return "low";
    case 1: return "curl";
    case 2: return "wrap";
    case 3: return "still";
    case 4: return "question";
    case 5: return "level";
    case 6: return "flag";
    case 7: return "twitch";
    case 8: return "bottle";
    default: return "unknown";
    }
}

const char *caz_vocal_name(uint8_t vocal)
{
    switch (vocal % 6u) {
    case 0: return "silent";
    case 1: return "mrrp";
    case 2: return "chirrup";
    case 3: return "purr";
    case 4: return "hiss";
    case 5: return "meow";
    default: return "unknown";
    }
}
