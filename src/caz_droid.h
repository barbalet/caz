#ifndef CAZ_DROID_H
#define CAZ_DROID_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "caz_body.h"

enum {
    CAZ_PORT_EYE_LUMA = 0x10,
    CAZ_PORT_EYE_MOTION = 0x11,
    CAZ_PORT_EYE_EDGE = 0x12,
    CAZ_PORT_EYE_COLOUR_TEMP = 0x13,

    CAZ_PORT_EAR_VOLUME = 0x20,
    CAZ_PORT_EAR_PITCH = 0x21,
    CAZ_PORT_EAR_BEARING = 0x22,
    CAZ_PORT_EAR_PATTERN = 0x23,

    CAZ_PORT_GAIT = 0x40,
    CAZ_PORT_HEAD_YAW = 0x41,
    CAZ_PORT_EAR_POSE = 0x42,
    CAZ_PORT_TAIL_POSE = 0x43,
    CAZ_PORT_VOCAL = 0x44,
    CAZ_PORT_EYELID = 0x45,

    CAZ_PORT_IMU_ROLL = 0x30,
    CAZ_PORT_IMU_PITCH = 0x31,
    CAZ_PORT_LIFTED = 0x32,
    CAZ_PORT_DROPPED = 0x33,
    CAZ_PORT_BATTERY = 0x34,
    CAZ_PORT_TERRAIN = 0x35,
    CAZ_PORT_ENERGY_SOURCE = 0x36,
    CAZ_PORT_CHARGER_BEARING = 0x37,
    CAZ_PORT_CHARGER_DISTANCE = 0x38,
    CAZ_PORT_CHARGER_SLOTS = 0x39,
    CAZ_PORT_JUNCTION_BEARING = 0x3a,
    CAZ_PORT_JUNCTION_DISTANCE = 0x3b,
    CAZ_PORT_SOLAR_LEVEL = 0x3c,

    CAZ_PORT_SKILL = 0x50,
    CAZ_PORT_SKILL_ARG = 0x51,
    CAZ_PORT_SKILL_STATUS = 0x52,
    CAZ_PORT_REFLEX_STATE = 0x53,
    CAZ_PORT_NAV_INTENT = 0x54,
    CAZ_PORT_NAV_STATUS = 0x55,

    CAZ_PORT_JOINT_INDEX = 0x60,
    CAZ_PORT_JOINT_ANGLE = 0x61,
    CAZ_PORT_JOINT_COMMIT = 0x62,

    CAZ_PORT_POSE_FRAME_INDEX = 0x70,
    CAZ_PORT_POSE_FRAME_VALUE = 0x71,
    CAZ_PORT_POSE_FRAME_FLAGS = 0x72,
    CAZ_PORT_POSE_FRAME_TIME = 0x73,
    CAZ_PORT_POSE_FRAME_COMMIT = 0x74
};

enum {
    CAZ_ENERGY_SOURCE_BATTERY = 0,
    CAZ_ENERGY_SOURCE_CHARGER = 1,
    CAZ_ENERGY_SOURCE_SOLAR = 2,
    CAZ_ENERGY_SOURCE_JUNCTION = 3
};

enum {
    CAZ_NAV_WANDER = 0,
    CAZ_NAV_CHARGER = 1,
    CAZ_NAV_SOLAR = 2,
    CAZ_NAV_JUNCTION = 3
};

enum {
    CAZ_NAV_STATUS_IDLE = 0,
    CAZ_NAV_STATUS_RUNNING = 1,
    CAZ_NAV_STATUS_BLOCKED = 2,
    CAZ_NAV_STATUS_DOCKED = 3,
    CAZ_NAV_STATUS_TAPPING = 4,
    CAZ_NAV_STATUS_SOLAR = 5
};

typedef enum CazScenario {
    CAZ_SCENARIO_KITCHEN = 0,
    CAZ_SCENARIO_FARMYARD,
    CAZ_SCENARIO_NIGHT_PARLOUR,
    CAZ_SCENARIO_HEDGEROW
} CazScenario;

typedef struct CazDroid {
    CazScenario scenario;
    CazBody body;
    uint32_t rng;
    uint64_t body_ticks;

    uint8_t eye_luma;
    uint8_t eye_motion;
    uint8_t eye_edge;
    uint8_t eye_colour_temp;

    uint8_t ear_volume;
    uint8_t ear_pitch;
    uint8_t ear_bearing;
    uint8_t ear_pattern;

    uint8_t energy_source;
    uint8_t charger_bearing;
    uint8_t charger_distance;
    uint8_t charger_slots;
    uint8_t junction_bearing;
    uint8_t junction_distance;
    uint8_t solar_level;
    uint8_t nav_intent;
    uint8_t nav_status;

    uint8_t gait;
    uint8_t head_yaw;
    uint8_t ear_pose;
    uint8_t tail_pose;
    uint8_t vocal;
    uint8_t eyelid;

    int16_t x;
    int16_t y;
    int16_t heading;
    int energy;
    int curiosity;
    int comfort;
} CazDroid;

void caz_droid_init(CazDroid *droid, CazScenario scenario, uint32_t seed);
void caz_droid_tick(CazDroid *droid);
uint8_t caz_droid_read_port(void *user, uint8_t port);
void caz_droid_write_port(void *user, uint8_t port, uint8_t value);
void caz_droid_print_report(const CazDroid *droid, FILE *out);

const char *caz_scenario_name(CazScenario scenario);
bool caz_scenario_parse(const char *name, CazScenario *scenario);
void caz_scenario_print_all(FILE *out);

const char *caz_port_name(uint8_t port);
const char *caz_pattern_name(uint8_t pattern);
const char *caz_gait_name(uint8_t gait);
const char *caz_ear_pose_name(uint8_t pose);
const char *caz_tail_pose_name(uint8_t pose);
const char *caz_vocal_name(uint8_t vocal);

#endif
