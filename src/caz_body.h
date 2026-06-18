#ifndef CAZ_BODY_H
#define CAZ_BODY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAZ_BODY_JOINT_COUNT 16u
#define CAZ_BODY_POSE_FRAME_COUNT 16u

typedef enum CazBodyJoint {
    CAZ_BODY_JOINT_HEAD_YAW = 0,
    CAZ_BODY_JOINT_HEAD_PITCH = 1,
    CAZ_BODY_JOINT_LEFT_SHOULDER = 2,
    CAZ_BODY_JOINT_RIGHT_SHOULDER = 3,
    CAZ_BODY_JOINT_LEFT_HIP = 4,
    CAZ_BODY_JOINT_RIGHT_HIP = 5,
    CAZ_BODY_JOINT_TAIL_BASE = 6,
    CAZ_BODY_JOINT_TAIL_TIP = 7,
    CAZ_BODY_JOINT_SPINE_HEIGHT = 8,
    CAZ_BODY_JOINT_SPINE_CURVE = 9,
    CAZ_BODY_JOINT_LEFT_KNEE = 10,
    CAZ_BODY_JOINT_RIGHT_KNEE = 11,
    CAZ_BODY_JOINT_LEFT_ELBOW = 12,
    CAZ_BODY_JOINT_RIGHT_ELBOW = 13,
    CAZ_BODY_JOINT_PAW_SPREAD = 14,
    CAZ_BODY_JOINT_BODY_ROLL = 15
} CazBodyJoint;

typedef enum CazBodySkill {
    CAZ_BODY_SKILL_NONE = 0,
    CAZ_BODY_SKILL_BALANCE = 1,
    CAZ_BODY_SKILL_REST = 2,
    CAZ_BODY_SKILL_SIT = 3,
    CAZ_BODY_SKILL_WALK = 4,
    CAZ_BODY_SKILL_CRAWL = 5,
    CAZ_BODY_SKILL_POUNCE = 6,
    CAZ_BODY_SKILL_SNIFF = 7,
    CAZ_BODY_SKILL_SCRATCH = 8,
    CAZ_BODY_SKILL_GROOM = 9,
    CAZ_BODY_SKILL_STRETCH = 10,
    CAZ_BODY_SKILL_STARTLE = 11,
    CAZ_BODY_SKILL_RECOVER = 12
} CazBodySkill;

typedef enum CazBodySkillStatus {
    CAZ_BODY_SKILL_IDLE = 0,
    CAZ_BODY_SKILL_READY = 1,
    CAZ_BODY_SKILL_RUNNING = 2,
    CAZ_BODY_SKILL_BLOCKED = 3,
    CAZ_BODY_SKILL_REFLEX = 4
} CazBodySkillStatus;

typedef enum CazBodyReflexState {
    CAZ_BODY_REFLEX_CLEAR = 0,
    CAZ_BODY_REFLEX_LOW_BATTERY = 1,
    CAZ_BODY_REFLEX_DROPPED = 2,
    CAZ_BODY_REFLEX_LIFTED = 3,
    CAZ_BODY_REFLEX_BALANCE = 4,
    CAZ_BODY_REFLEX_TERRAIN_CAUTION = 5
} CazBodyReflexState;

typedef struct CazBody {
    uint8_t imu_roll;
    uint8_t imu_pitch;
    uint8_t lifted;
    uint8_t dropped;
    uint8_t battery;
    uint8_t terrain;

    uint8_t requested_gait;
    uint8_t requested_head_yaw;
    uint8_t requested_ear_pose;
    uint8_t requested_tail_pose;
    uint8_t requested_vocal;
    uint8_t requested_eyelid;
    uint8_t requested_skill;

    uint8_t gait;
    uint8_t head_yaw;
    uint8_t ear_pose;
    uint8_t tail_pose;
    uint8_t vocal;
    uint8_t eyelid;

    uint8_t active_skill;
    uint8_t skill_arg;
    uint8_t skill_status;
    uint8_t reflex_state;

    uint8_t selected_joint;
    uint8_t pending_joint_angle;
    uint8_t requested_joint_targets[CAZ_BODY_JOINT_COUNT];
    uint8_t joint_targets[CAZ_BODY_JOINT_COUNT];
    uint8_t joint_values[CAZ_BODY_JOINT_COUNT];

    uint8_t pose_frame_index;
    uint8_t pose_frame_value;
    uint8_t pose_frame_flags;
    uint8_t pose_frame_time;
    uint8_t pose_frame_buffer[CAZ_BODY_POSE_FRAME_COUNT];
    uint8_t pose_frame[CAZ_BODY_POSE_FRAME_COUNT];
} CazBody;

void caz_body_init(CazBody *body);
void caz_body_set_normalized_sensors(CazBody *body,
                                     uint8_t imu_roll,
                                     uint8_t imu_pitch,
                                     uint8_t lifted,
                                     uint8_t dropped,
                                     uint8_t battery,
                                     uint8_t terrain);
void caz_body_apply_reflexes(CazBody *body);
void caz_body_tick(CazBody *body);
void caz_body_reset_skill_library(void);
bool caz_body_load_skill_dir(const char *path, char *error, size_t error_length);
bool caz_body_read_port(const CazBody *body, uint8_t port, uint8_t *value);
bool caz_body_write_port(CazBody *body, uint8_t port, uint8_t value);
uint8_t caz_body_joint_value(const CazBody *body, uint8_t joint);
uint8_t caz_body_joint_target(const CazBody *body, uint8_t joint);
const char *caz_body_skill_name(uint8_t skill);
const char *caz_body_skill_status_name(uint8_t status);
const char *caz_body_reflex_state_name(uint8_t reflex_state);
const char *caz_body_joint_name(uint8_t joint);

#endif
