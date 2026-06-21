#ifndef CAZ_ENV_H
#define CAZ_ENV_H

#include <stddef.h>
#include <stdint.h>

#include "../../src/caz_cpu.h"
#include "../../src/caz_loader.h"

#define CAZ_ENV_ROOM_WIDTH_FT 30.0f
#define CAZ_ENV_ROOM_LENGTH_FT 60.0f
#define CAZ_ENV_ROOM_HEIGHT_FT 15.0f
#define CAZ_ENV_DROID_COUNT 20
#define CAZ_ENV_FIXTURE_COUNT 21
#define CAZ_ENV_CHARGE_SLOT_COUNT 8
#define CAZ_ENV_PROGRAM_COUNT 10

typedef enum CazEnvFixtureType {
    CAZ_ENV_FIXTURE_CHARGER = 1,
    CAZ_ENV_FIXTURE_CAT_BED = 2,
    CAZ_ENV_FIXTURE_TREE_TALL = 3,
    CAZ_ENV_FIXTURE_TREE_MID = 4,
    CAZ_ENV_FIXTURE_TREE_COMPACT = 5,
    CAZ_ENV_FIXTURE_SCRATCH_POST = 6,
    CAZ_ENV_FIXTURE_JUNCTION_BOX = 7
} CazEnvFixtureType;

typedef enum CazEnvDroidMode {
    CAZ_ENV_DROID_WANDER = 0,
    CAZ_ENV_DROID_PROGRAM = 1,
    CAZ_ENV_DROID_RETURN_TO_CHARGE = 2,
    CAZ_ENV_DROID_CHARGING = 3,
    CAZ_ENV_DROID_DEPLETED = 4,
    CAZ_ENV_DROID_SOLAR_FORAGE = 5,
    CAZ_ENV_DROID_TAP_JUNCTION = 6
} CazEnvDroidMode;

typedef enum CazEnvEnergySource {
    CAZ_ENV_ENERGY_BATTERY = 0,
    CAZ_ENV_ENERGY_CHARGER = 1,
    CAZ_ENV_ENERGY_SOLAR = 2,
    CAZ_ENV_ENERGY_JUNCTION = 3
} CazEnvEnergySource;

typedef enum CazEnvNavCause {
    CAZ_ENV_NAV_CAUSE_NONE = 0,
    CAZ_ENV_NAV_CAUSE_BYTECODE = 1,
    CAZ_ENV_NAV_CAUSE_SUPERVISOR = 2,
    CAZ_ENV_NAV_CAUSE_FAILURE = 3
} CazEnvNavCause;

typedef struct CazEnvFixtureSnapshot {
    uint8_t type;
    float x;
    float y;
    float z;
    float width;
    float length;
    float height;
    float yaw;
} CazEnvFixtureSnapshot;

typedef struct CazEnvDroidSnapshot {
    float x;
    float z;
    float yaw;
    float charge;
    float feral;
    float solar_gain;
    float tap_gain;
    float speed;
    uint8_t program;
    uint8_t gait;
    uint8_t mode;
    uint8_t charging_slot;
    uint8_t energy_source;
    uint8_t bytecode_loaded;
    uint8_t bytecode_stepping_enabled;
    uint8_t bytecode_halted;
    uint8_t bytecode_faulted;
    uint8_t nav_intent;
    uint8_t nav_status;
    uint8_t nav_cause;
    uint8_t skill;
    uint8_t bytecode_gait;
    uint8_t head_yaw;
    uint8_t ear_pose;
    uint8_t tail_pose;
    uint8_t vocal;
    uint8_t eyelid;
    uint8_t reflex_state;
    uint8_t bytecode_program;
    uint32_t nav_transition_count;
    uint64_t bytecode_instructions;
    uint64_t bytecode_cycles;
} CazEnvDroidSnapshot;

typedef struct CazEnvBytecodeOutput {
    uint8_t nav_intent;
    uint8_t skill;
    uint8_t skill_arg;
    uint8_t gait;
    uint8_t head_yaw;
    uint8_t ear_pose;
    uint8_t tail_pose;
    uint8_t vocal;
    uint8_t eyelid;
    uint8_t selected_joint;
    uint8_t pending_joint_angle;
} CazEnvBytecodeOutput;

typedef struct CazEnvBytecodeRuntime {
    CazCpu cpu;
    CazProgramImage image;
    CazEnvBytecodeOutput output;
    struct CazEnvState *state;
    uint8_t assigned_program;
    uint8_t droid_index;
    uint8_t image_loaded;
    uint8_t stepping_enabled;
    uint8_t faulted;
} CazEnvBytecodeRuntime;

typedef struct CazEnvFixture {
    uint8_t type;
    float x;
    float y;
    float z;
    float width;
    float length;
    float height;
    float yaw;
} CazEnvFixture;

typedef struct CazEnvDroid {
    float x;
    float z;
    float yaw;
    float target_x;
    float target_z;
    float charge;
    float feral;
    float solar_gain;
    float tap_gain;
    float speed;
    float phase;
    uint8_t program;
    uint8_t gait;
    uint8_t mode;
    uint8_t charging_slot;
    uint8_t energy_source;
    uint8_t nav_status;
    uint8_t nav_cause;
    uint8_t last_nav_intent;
    uint8_t last_nav_status;
    uint8_t last_nav_cause;
    uint32_t nav_transition_count;
    CazEnvBytecodeRuntime bytecode;
} CazEnvDroid;

typedef struct CazEnvState {
    uint32_t rng;
    float elapsed_seconds;
    CazEnvFixture fixtures[CAZ_ENV_FIXTURE_COUNT];
    CazEnvDroid droids[CAZ_ENV_DROID_COUNT];
    int8_t charge_slots[CAZ_ENV_CHARGE_SLOT_COUNT];
} CazEnvState;

void caz_env_init(CazEnvState *state, uint32_t seed);
int caz_env_load_programs(CazEnvState *state, const char *program_dir, char *error, size_t error_length);
void caz_env_step(CazEnvState *state, float dt_seconds);
int caz_env_fixture_count(void);
int caz_env_droid_count(void);
void caz_env_fixture_snapshot(const CazEnvState *state, int index, CazEnvFixtureSnapshot *out_snapshot);
void caz_env_droid_snapshot(const CazEnvState *state, int index, CazEnvDroidSnapshot *out_snapshot);
const char *caz_env_program_name(uint8_t program);
const char *caz_env_bytecode_program_name(const CazEnvState *state, int index);
uint8_t caz_env_debug_read_port(const CazEnvState *state, int index, uint8_t port);

#endif
