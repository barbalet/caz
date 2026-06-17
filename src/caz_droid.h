#ifndef CAZ_DROID_H
#define CAZ_DROID_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
    CAZ_PORT_EYELID = 0x45
};

typedef enum CazScenario {
    CAZ_SCENARIO_KITCHEN = 0,
    CAZ_SCENARIO_FARMYARD,
    CAZ_SCENARIO_NIGHT_PARLOUR,
    CAZ_SCENARIO_HEDGEROW
} CazScenario;

typedef struct CazDroid {
    CazScenario scenario;
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
