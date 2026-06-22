#include "../c_core/caz_env.h"
#include "../../src/caz_droid.h"

#include <dirent.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define RECOVERY_TRACE_COUNT 8u

typedef enum HarnessMode {
    HARNESS_MODE_PROBES,
    HARNESS_MODE_REGRESSION,
    HARNESS_MODE_STRESS,
    HARNESS_MODE_SHORT_COMPAT,
    HARNESS_MODE_SHORT_STRICT,
    HARNESS_MODE_LONG_COMPAT,
    HARNESS_MODE_LONG_STRICT,
    HARNESS_MODE_MATRIX
} HarnessMode;

typedef enum MatrixCase {
    MATRIX_CASE_BASELINE,
    MATRIX_CASE_LOW_SUN,
    MATRIX_CASE_FULL_CHARGER,
    MATRIX_CASE_DISTANT_JUNCTION,
    MATRIX_CASE_HIGH_OBSTACLE,
    MATRIX_CASE_COUNT
} MatrixCase;

typedef struct HarnessOptions {
    HarnessMode mode;
    int days;
    int seed_count;
    MatrixCase matrix_case;
} HarnessOptions;

typedef struct RecoveryTrace {
    long step;
    float charge;
    uint8_t mode;
    uint8_t energy_source;
    uint8_t nav_intent;
    uint8_t nav_status;
    uint8_t nav_cause;
} RecoveryTrace;

typedef struct DroidTrack {
    float minimum_charge;
    unsigned ever_charging;
    unsigned ever_depleted;
    unsigned ever_returning;
    unsigned ever_solar;
    unsigned ever_tapping;
    unsigned ever_zero_charge;
    unsigned ever_nav_charger;
    unsigned ever_nav_solar;
    unsigned ever_nav_junction;
    unsigned ever_bytecode_recovery;
    unsigned ever_supervisor_recovery;
    unsigned recovery_trace_count;
    unsigned recovery_trace_next;
    RecoveryTrace recovery_trace[RECOVERY_TRACE_COUNT];
} DroidTrack;

typedef struct FailureReport {
    int captured;
    uint32_t seed;
    long step;
    int droid_index;
    char program_name[CAZ_PROGRAM_NAME_MAX];
    float charge;
    float x;
    float z;
    float charger_distance;
    float junction_distance;
    float charger_gain;
    float passive_solar_gain;
    float nav_solar_gain;
    float fallback_solar_gain;
    float tap_gain;
    float nav_tap_gain;
    float fallback_tap_gain;
    uint8_t mode;
    uint8_t energy_source;
    uint8_t nav_intent;
    uint8_t nav_status;
    uint8_t nav_cause;
    uint8_t charger_slots;
    uint8_t obstacle_state;
    uint32_t blocked_movement_count;
    uint32_t blocked_junction_count;
    unsigned recent_count;
    RecoveryTrace recent[RECOVERY_TRACE_COUNT];
} FailureReport;

static const char *mode_name(unsigned mode)
{
    switch (mode) {
    case CAZ_ENV_DROID_WANDER:
        return "wander";
    case CAZ_ENV_DROID_PROGRAM:
        return "program";
    case CAZ_ENV_DROID_RETURN_TO_CHARGE:
        return "return";
    case CAZ_ENV_DROID_CHARGING:
        return "charging";
    case CAZ_ENV_DROID_DEPLETED:
        return "depleted";
    case CAZ_ENV_DROID_SOLAR_FORAGE:
        return "solar";
    case CAZ_ENV_DROID_TAP_JUNCTION:
        return "tap";
    default:
        return "unknown";
    }
}

static const char *fixture_name(unsigned type)
{
    switch (type) {
    case CAZ_ENV_FIXTURE_CHARGER:
        return "charger";
    case CAZ_ENV_FIXTURE_CAT_BED:
        return "bed";
    case CAZ_ENV_FIXTURE_TREE_TALL:
        return "tree_tall";
    case CAZ_ENV_FIXTURE_TREE_MID:
        return "tree_mid";
    case CAZ_ENV_FIXTURE_TREE_COMPACT:
        return "tree_compact";
    case CAZ_ENV_FIXTURE_SCRATCH_POST:
        return "scratch_post";
    case CAZ_ENV_FIXTURE_JUNCTION_BOX:
        return "junction";
    default:
        return "unknown";
    }
}

static const char *harness_mode_name(HarnessMode mode)
{
    switch (mode) {
    case HARNESS_MODE_PROBES:
        return "probes";
    case HARNESS_MODE_REGRESSION:
        return "regression";
    case HARNESS_MODE_STRESS:
        return "stress";
    case HARNESS_MODE_SHORT_COMPAT:
        return "short-compat";
    case HARNESS_MODE_SHORT_STRICT:
        return "short-strict";
    case HARNESS_MODE_LONG_COMPAT:
        return "long-compat";
    case HARNESS_MODE_LONG_STRICT:
        return "long-strict";
    case HARNESS_MODE_MATRIX:
        return "matrix";
    default:
        return "unknown";
    }
}

static const char *fallback_policy_name(HarnessMode mode)
{
    switch (mode) {
    case HARNESS_MODE_PROBES:
        return "targeted-probes";
    case HARNESS_MODE_REGRESSION:
        return "bytecode-regression";
    case HARNESS_MODE_STRESS:
        return "stress-matrix";
    case HARNESS_MODE_SHORT_COMPAT:
    case HARNESS_MODE_LONG_COMPAT:
        return "allowed-counted";
    case HARNESS_MODE_SHORT_STRICT:
    case HARNESS_MODE_LONG_STRICT:
    case HARNESS_MODE_MATRIX:
        return "fail-on-use";
    default:
        return "unknown";
    }
}

static const char *strategy_name(float feral)
{
    return feral >= 0.62f ? "feral" : "house";
}

static int is_strict_mode(HarnessMode mode)
{
    return mode == HARNESS_MODE_SHORT_STRICT ||
           mode == HARNESS_MODE_LONG_STRICT ||
           mode == HARNESS_MODE_MATRIX;
}

static const char *matrix_case_name(MatrixCase matrix_case)
{
    switch (matrix_case) {
    case MATRIX_CASE_BASELINE:
        return "baseline";
    case MATRIX_CASE_LOW_SUN:
        return "low-sun";
    case MATRIX_CASE_FULL_CHARGER:
        return "full-charger";
    case MATRIX_CASE_DISTANT_JUNCTION:
        return "distant-junction";
    case MATRIX_CASE_HIGH_OBSTACLE:
        return "high-obstacle";
    default:
        return "unknown";
    }
}

static float distance2f(float ax, float az, float bx, float bz)
{
    const float dx = ax - bx;
    const float dz = az - bz;
    return dx * dx + dz * dz;
}

static float charger_distance_for_droid(const CazEnvState *state, const CazEnvDroid *droid)
{
    return sqrtf(distance2f(droid->x, droid->z, state->fixtures[0].x, state->fixtures[0].z));
}

static float nearest_junction_distance_for_droid(const CazEnvState *state, const CazEnvDroid *droid)
{
    float nearest = FLT_MAX;
    for (int index = 0; index < caz_env_fixture_count(); index++) {
        const CazEnvFixture *fixture = &state->fixtures[index];
        if (fixture->type != CAZ_ENV_FIXTURE_JUNCTION_BOX) {
            continue;
        }
        const float distance = sqrtf(distance2f(droid->x, droid->z, fixture->x, fixture->z));
        if (distance < nearest) {
            nearest = distance;
        }
    }
    return nearest == FLT_MAX ? -1.0f : nearest;
}

static int parse_mode_name(const char *name, HarnessMode *out_mode)
{
    if (strcmp(name, "probes") == 0) {
        *out_mode = HARNESS_MODE_PROBES;
    } else if (strcmp(name, "regression") == 0) {
        *out_mode = HARNESS_MODE_REGRESSION;
    } else if (strcmp(name, "stress") == 0) {
        *out_mode = HARNESS_MODE_STRESS;
    } else if (strcmp(name, "short-compat") == 0 || strcmp(name, "compat") == 0) {
        *out_mode = HARNESS_MODE_SHORT_COMPAT;
    } else if (strcmp(name, "short-strict") == 0 || strcmp(name, "strict") == 0) {
        *out_mode = HARNESS_MODE_SHORT_STRICT;
    } else if (strcmp(name, "long-compat") == 0) {
        *out_mode = HARNESS_MODE_LONG_COMPAT;
    } else if (strcmp(name, "long-strict") == 0) {
        *out_mode = HARNESS_MODE_LONG_STRICT;
    } else if (strcmp(name, "matrix") == 0 || strcmp(name, "long-matrix") == 0) {
        *out_mode = HARNESS_MODE_MATRIX;
    } else {
        return 0;
    }
    return 1;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--mode probes|regression|stress|short-compat|short-strict|long-compat|long-strict|matrix] [days] [seeds]\n"
            "default mode is short-strict; use short-compat only for labeled compatibility diagnostics\n",
            program);
}

static int parse_options(int argc, char **argv, HarnessOptions *options)
{
    int positional_count = 0;
    int days_was_set = 0;
    int seeds_was_set = 0;

    options->mode = HARNESS_MODE_SHORT_STRICT;
    options->days = 14;
    options->seed_count = 3;
    options->matrix_case = MATRIX_CASE_BASELINE;

    for (int index = 1; index < argc; index++) {
        const char *arg = argv[index];
        if (strcmp(arg, "--mode") == 0) {
            if (index + 1 >= argc || !parse_mode_name(argv[index + 1], &options->mode)) {
                return 0;
            }
            index++;
        } else if (strncmp(arg, "--mode=", 7) == 0) {
            if (!parse_mode_name(arg + 7, &options->mode)) {
                return 0;
            }
        } else if (strcmp(arg, "--probes") == 0) {
            options->mode = HARNESS_MODE_PROBES;
        } else if (strcmp(arg, "--regression") == 0) {
            options->mode = HARNESS_MODE_REGRESSION;
        } else if (strcmp(arg, "--stress") == 0) {
            options->mode = HARNESS_MODE_STRESS;
        } else if (strncmp(arg, "--", 2) == 0) {
            return 0;
        } else if (positional_count == 0) {
            options->days = atoi(arg);
            days_was_set = 1;
            positional_count++;
        } else if (positional_count == 1) {
            options->seed_count = atoi(arg);
            seeds_was_set = 1;
            positional_count++;
        } else {
            return 0;
        }
    }

    if ((options->mode == HARNESS_MODE_LONG_COMPAT || options->mode == HARNESS_MODE_LONG_STRICT) &&
        !days_was_set) {
        options->days = 30;
    }
    if (options->mode == HARNESS_MODE_MATRIX && !days_was_set) {
        options->days = 30;
    }
    if ((options->mode == HARNESS_MODE_LONG_COMPAT || options->mode == HARNESS_MODE_LONG_STRICT) &&
        !seeds_was_set) {
        options->seed_count = 5;
    }
    if (options->mode == HARNESS_MODE_MATRIX && !seeds_was_set) {
        options->seed_count = 5;
    }
    if (options->mode == HARNESS_MODE_PROBES ||
        options->mode == HARNESS_MODE_REGRESSION ||
        options->mode == HARNESS_MODE_STRESS) {
        options->days = 0;
        options->seed_count = 0;
    }
    if (options->mode != HARNESS_MODE_PROBES &&
        options->mode != HARNESS_MODE_REGRESSION &&
        options->mode != HARNESS_MODE_STRESS &&
        (options->days <= 0 || options->seed_count <= 0)) {
        return 0;
    }
    return 1;
}

static void count_fixtures(const CazEnvState *state)
{
    int counts[8] = {0};
    for (int index = 0; index < caz_env_fixture_count(); index++) {
        if (state->fixtures[index].type < 8u) {
            counts[state->fixtures[index].type]++;
        }
    }

    printf("fixture-counts");
    for (int type = 1; type < 8; type++) {
        printf(" %s=%d", fixture_name((unsigned)type), counts[type]);
    }
    printf("\n");
}

static void count_bytecode_runtimes(const CazEnvState *state)
{
    int assigned = 0;
    int loaded = 0;
    int stepping = 0;
    int halted = 0;
    int faulted = 0;
    uint64_t instructions = 0u;

    for (int index = 0; index < caz_env_droid_count(); index++) {
        const CazEnvBytecodeRuntime *runtime = &state->droids[index].bytecode;
        assigned += runtime->image.name[0] != '\0' ? 1 : 0;
        loaded += runtime->image_loaded ? 1 : 0;
        stepping += runtime->stepping_enabled ? 1 : 0;
        halted += runtime->cpu.halted ? 1 : 0;
        faulted += runtime->faulted ? 1 : 0;
        instructions += runtime->cpu.instructions;
    }

    printf("bytecode-runtimes assigned=%d loaded=%d stepping=%d halted=%d faulted=%d instructions=%llu\n",
           assigned,
           loaded,
           stepping,
           halted,
           faulted,
           (unsigned long long)instructions);
}

static void print_sample_ports(const CazEnvState *state)
{
    printf("sample-ports droid=0 eye=(luma=%u motion=%u edge=%u colour=%u) ear=(volume=%u pitch=%u bearing=%u pattern=%u) body=(roll=%u pitch=%u lifted=%u dropped=%u terrain=%u reflex=%u) survival=(battery=%u charger=%u/%u/%u junction=%u/%u solar=%u strategy=%u energy=%u nav=%u/%u)\n",
           caz_env_debug_read_port(state, 0, CAZ_PORT_EYE_LUMA),
           caz_env_debug_read_port(state, 0, CAZ_PORT_EYE_MOTION),
           caz_env_debug_read_port(state, 0, CAZ_PORT_EYE_EDGE),
           caz_env_debug_read_port(state, 0, CAZ_PORT_EYE_COLOUR_TEMP),
           caz_env_debug_read_port(state, 0, CAZ_PORT_EAR_VOLUME),
           caz_env_debug_read_port(state, 0, CAZ_PORT_EAR_PITCH),
           caz_env_debug_read_port(state, 0, CAZ_PORT_EAR_BEARING),
           caz_env_debug_read_port(state, 0, CAZ_PORT_EAR_PATTERN),
           caz_env_debug_read_port(state, 0, CAZ_PORT_IMU_ROLL),
           caz_env_debug_read_port(state, 0, CAZ_PORT_IMU_PITCH),
           caz_env_debug_read_port(state, 0, CAZ_PORT_LIFTED),
           caz_env_debug_read_port(state, 0, CAZ_PORT_DROPPED),
           caz_env_debug_read_port(state, 0, CAZ_PORT_TERRAIN),
           caz_env_debug_read_port(state, 0, CAZ_PORT_REFLEX_STATE),
           caz_env_debug_read_port(state, 0, CAZ_PORT_BATTERY),
           caz_env_debug_read_port(state, 0, CAZ_PORT_CHARGER_BEARING),
           caz_env_debug_read_port(state, 0, CAZ_PORT_CHARGER_DISTANCE),
           caz_env_debug_read_port(state, 0, CAZ_PORT_CHARGER_SLOTS),
           caz_env_debug_read_port(state, 0, CAZ_PORT_JUNCTION_BEARING),
           caz_env_debug_read_port(state, 0, CAZ_PORT_JUNCTION_DISTANCE),
           caz_env_debug_read_port(state, 0, CAZ_PORT_SOLAR_LEVEL),
           caz_env_debug_read_port(state, 0, CAZ_PORT_STRATEGY_TENDENCY),
           caz_env_debug_read_port(state, 0, CAZ_PORT_ENERGY_SOURCE),
           caz_env_debug_read_port(state, 0, CAZ_PORT_NAV_INTENT),
           caz_env_debug_read_port(state, 0, CAZ_PORT_NAV_STATUS));
}

static void print_sample_output(const CazEnvState *state, int index)
{
    const CazEnvDroid *droid = &state->droids[index];
    const CazEnvBytecodeRuntime *runtime = &droid->bytecode;
    printf("sample-output droid=%d program=%s strategy=%s/%.2f output=(nav=%u status=%u cause=%u gait=%u skill=%u head=%u ear=%u tail=%u vocal=%u eyelid=%u) motion=(mode=%u speed=%.2f target=%.2f/%.2f obstacle=%u blocked=%u junction_blocked=%u transitions=%u)\n",
           index,
           caz_env_bytecode_program_name(state, index),
           strategy_name(droid->feral),
           droid->feral,
           runtime->output.nav_intent,
           droid->nav_status,
           droid->nav_cause,
           runtime->output.gait,
           runtime->output.skill,
           runtime->output.head_yaw,
           runtime->output.ear_pose,
           runtime->output.tail_pose,
           runtime->output.vocal,
           runtime->output.eyelid,
           droid->mode,
           droid->speed,
           droid->target_x,
           droid->target_z,
           droid->obstacle_state,
           droid->blocked_movement_count,
           droid->blocked_junction_count,
           droid->nav_transition_count);
}

static uint64_t supervisor_metric_total(const CazEnvSupervisorMetrics *metrics)
{
    return metrics->charger_returns +
           metrics->solar_forages +
           metrics->junction_taps +
           metrics->charger_loiters +
           metrics->speed_overrides +
           metrics->gait_overrides;
}

static void record_recovery_trace(DroidTrack *track, long step, const CazEnvDroid *droid)
{
    const int recovery_context = droid->bytecode.output.nav_intent != CAZ_NAV_WANDER ||
                                 droid->nav_cause != CAZ_ENV_NAV_CAUSE_NONE ||
                                 droid->mode == CAZ_ENV_DROID_RETURN_TO_CHARGE ||
                                 droid->mode == CAZ_ENV_DROID_CHARGING ||
                                 droid->mode == CAZ_ENV_DROID_DEPLETED ||
                                 droid->mode == CAZ_ENV_DROID_SOLAR_FORAGE ||
                                 droid->mode == CAZ_ENV_DROID_TAP_JUNCTION ||
                                 droid->energy_source != CAZ_ENV_ENERGY_BATTERY;
    if (!recovery_context) {
        return;
    }

    RecoveryTrace *trace = &track->recovery_trace[track->recovery_trace_next];
    trace->step = step;
    trace->charge = droid->charge;
    trace->mode = droid->mode;
    trace->energy_source = droid->energy_source;
    trace->nav_intent = droid->bytecode.output.nav_intent;
    trace->nav_status = droid->nav_status;
    trace->nav_cause = droid->nav_cause;
    track->recovery_trace_next = (track->recovery_trace_next + 1u) % RECOVERY_TRACE_COUNT;
    if (track->recovery_trace_count < RECOVERY_TRACE_COUNT) {
        track->recovery_trace_count++;
    }
}

static void capture_failure(FailureReport *report,
                            const CazEnvState *state,
                            const DroidTrack *track,
                            uint32_t seed,
                            long step,
                            int droid_index)
{
    if (report->captured) {
        return;
    }

    const CazEnvDroid *droid = &state->droids[droid_index];
    report->captured = 1;
    report->seed = seed;
    report->step = step;
    report->droid_index = droid_index;
    snprintf(report->program_name, sizeof(report->program_name), "%s", caz_env_bytecode_program_name(state, droid_index));
    report->charge = droid->charge;
    report->x = droid->x;
    report->z = droid->z;
    report->charger_distance = charger_distance_for_droid(state, droid);
    report->junction_distance = nearest_junction_distance_for_droid(state, droid);
    report->charger_gain = droid->charger_gain;
    report->passive_solar_gain = droid->passive_solar_gain;
    report->nav_solar_gain = droid->nav_solar_gain;
    report->fallback_solar_gain = droid->fallback_solar_gain;
    report->tap_gain = droid->tap_gain;
    report->nav_tap_gain = droid->nav_tap_gain;
    report->fallback_tap_gain = droid->fallback_tap_gain;
    report->mode = droid->mode;
    report->energy_source = droid->energy_source;
    report->nav_intent = droid->bytecode.output.nav_intent;
    report->nav_status = droid->nav_status;
    report->nav_cause = droid->nav_cause;
    report->charger_slots = caz_env_debug_read_port(state, droid_index, CAZ_PORT_CHARGER_SLOTS);
    report->obstacle_state = droid->obstacle_state;
    report->blocked_movement_count = droid->blocked_movement_count;
    report->blocked_junction_count = droid->blocked_junction_count;

    report->recent_count = track->recovery_trace_count;
    const unsigned start = track->recovery_trace_count < RECOVERY_TRACE_COUNT
                         ? 0u
                         : track->recovery_trace_next;
    for (unsigned index = 0; index < report->recent_count; index++) {
        report->recent[index] = track->recovery_trace[(start + index) % RECOVERY_TRACE_COUNT];
    }
}

static void fprint_failure_report(FILE *out, const FailureReport *report)
{
    if (!report->captured) {
        return;
    }

    fprintf(out,
            "first-failure seed=0x%08x step=%ld droid=%d program=%s charge=%.6f pos=%.2f/%.2f mode=%s energy=%u output=(nav=%u status=%u cause=%u) charger_slots=%u obstacle=%u blocked=%u junction_blocked=%u distances=(charger=%.2f junction=%.2f) gains=(charger=%.6f passive=%.6f requested_solar=%.6f fallback_solar=%.6f tap=%.6f nav_tap=%.6f fallback_tap=%.6f)\n",
            report->seed,
            report->step,
            report->droid_index,
            report->program_name,
            report->charge,
            report->x,
            report->z,
            mode_name(report->mode),
            report->energy_source,
            report->nav_intent,
            report->nav_status,
            report->nav_cause,
            report->charger_slots,
            report->obstacle_state,
            report->blocked_movement_count,
            report->blocked_junction_count,
            report->charger_distance,
            report->junction_distance,
            report->charger_gain,
            report->passive_solar_gain,
            report->nav_solar_gain,
            report->fallback_solar_gain,
            report->tap_gain,
            report->nav_tap_gain,
            report->fallback_tap_gain);
    fprintf(out, "first-failure-recent");
    for (unsigned index = 0; index < report->recent_count; index++) {
        const RecoveryTrace *trace = &report->recent[index];
        fprintf(out,
                " [step=%ld charge=%.4f mode=%s energy=%u nav=%u status=%u cause=%u]",
                trace->step,
                trace->charge,
                mode_name(trace->mode),
                trace->energy_source,
                trace->nav_intent,
                trace->nav_status,
                trace->nav_cause);
    }
    fprintf(out, "\n");
}

static void print_failure_report(const FailureReport *report)
{
    fprint_failure_report(stdout, report);
}

static void write_failure_artifact(const FailureReport *report, const HarnessOptions *options)
{
    char path[256];
    FILE *file;

    if (!report->captured) {
        return;
    }

    mkdir("build", 0777);
    mkdir("build/cazenv-failures", 0777);
    snprintf(path,
             sizeof(path),
             "build/cazenv-failures/%s-%s-seed-%08x-droid-%02d-step-%ld.txt",
             harness_mode_name(options->mode),
             matrix_case_name(options->matrix_case),
             report->seed,
             report->droid_index,
             report->step);
    file = fopen(path, "w");
    if (file == NULL) {
        printf("first-failure-artifact path=%s result=ERROR\n", path);
        return;
    }
    fprintf(file,
            "artifact=cazenv-first-failure mode=%s fallback=%s matrix_case=%s days=%d seeds=%d\n",
            harness_mode_name(options->mode),
            fallback_policy_name(options->mode),
            matrix_case_name(options->matrix_case),
            options->days,
            options->seed_count);
    fprint_failure_report(file, report);
    fclose(file);
    printf("first-failure-artifact path=%s result=written\n", path);
}

static int load_probe_programs(CazEnvState *state, char *load_error, size_t load_error_length)
{
    if (!caz_env_load_programs(state, "programs", load_error, load_error_length)) {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }
    return 1;
}

static void isolate_probe_droid(CazEnvState *state, int droid_index)
{
    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        CazEnvDroid *droid = &state->droids[index];
        if (index == droid_index) {
            continue;
        }
        droid->x = -CAZ_ENV_ROOM_WIDTH_FT * 0.45f + (float)(index % 5) * 1.2f;
        droid->z = -CAZ_ENV_ROOM_LENGTH_FT * 0.45f + (float)(index / 5) * 1.2f;
        droid->target_x = droid->x;
        droid->target_z = droid->z;
        droid->speed = 0.0f;
        droid->mode = CAZ_ENV_DROID_PROGRAM;
        droid->bytecode.stepping_enabled = 0u;
        droid->bytecode.output.gait = 0u;
        droid->bytecode.output.skill = CAZ_BODY_SKILL_REST;
    }
}

static int charge_slots_valid(const CazEnvState *state)
{
    int seen[CAZ_ENV_DROID_COUNT] = {0};
    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        const int owner = state->charge_slots[slot];
        if (owner < 0) {
            continue;
        }
        if (owner >= CAZ_ENV_DROID_COUNT || seen[owner]) {
            return 0;
        }
        if (state->droids[owner].charging_slot != (uint8_t)slot) {
            return 0;
        }
        seen[owner] = 1;
    }
    for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
        const uint8_t slot = state->droids[index].charging_slot;
        if (slot < CAZ_ENV_CHARGE_SLOT_COUNT && state->charge_slots[slot] != index) {
            return 0;
        }
    }
    return 1;
}

static void set_charge_slot_owner(CazEnvState *state, int slot, int droid_index, float charge)
{
    CazEnvDroid *droid = &state->droids[droid_index];
    state->charge_slots[slot] = (int8_t)droid_index;
    droid->x = state->fixtures[0].x + ((float)(slot % 4) - 1.5f) * 0.28f;
    droid->z = state->fixtures[0].z + ((float)(slot / 4) - 0.5f) * 0.28f;
    droid->target_x = droid->x;
    droid->target_z = droid->z;
    droid->charge = charge;
    droid->feral = 0.10f;
    droid->mode = CAZ_ENV_DROID_CHARGING;
    droid->charging_slot = (uint8_t)slot;
    droid->energy_source = CAZ_ENV_ENERGY_CHARGER;
    droid->nav_cause = CAZ_ENV_NAV_CAUSE_BYTECODE;
    droid->nav_status = CAZ_NAV_STATUS_DOCKED;
    droid->bytecode.output.nav_intent = CAZ_NAV_CHARGER;
    droid->bytecode.output.skill = CAZ_BODY_SKILL_REST;
    droid->bytecode.output.gait = 0u;
    droid->speed = 0.0f;
}

static void park_probe_droid(CazEnvState *state, int index, float x, float z)
{
    CazEnvDroid *droid = &state->droids[index];
    droid->x = x;
    droid->z = z;
    droid->target_x = x;
    droid->target_z = z;
    droid->speed = 0.0f;
    droid->mode = CAZ_ENV_DROID_PROGRAM;
    droid->charging_slot = 255u;
    droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
    droid->nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
    droid->nav_status = CAZ_NAV_STATUS_IDLE;
}

static void fill_charger_for_probe(CazEnvState *state, float release_charge)
{
    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        set_charge_slot_owner(state, slot, slot, slot == 0 ? release_charge : 0.70f);
    }
}

static int has_caz_suffix(const char *name)
{
    const size_t length = strlen(name);
    return length > 4u && strcmp(name + length - 4u, ".caz") == 0;
}

static int registry_name_count(const char *name)
{
    int count = 0;
    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        const CazProgramMetadata *metadata = caz_loader_program_metadata_at(index);
        if (metadata != NULL && strcmp(metadata->name, name) == 0) {
            count++;
        }
    }
    return count;
}

static int program_source_contains(const CazProgramMetadata *metadata, const char *needle, int *contains)
{
    char path[CAZ_PROGRAM_PATH_MAX];
    FILE *file;
    char *buffer;
    long size;
    size_t read_count;
    int ok = 1;

    *contains = 0;
    if (!caz_loader_program_path(metadata->kind, "programs", path, sizeof(path))) {
        fprintf(stderr, "program-registry could not build source path for %s\n", metadata->name);
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "program-registry could not open source for %s: %s\n", metadata->name, path);
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "program-registry could not seek source for %s\n", metadata->name);
        return 0;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        fprintf(stderr, "program-registry could not size source for %s\n", metadata->name);
        return 0;
    }
    rewind(file);
    buffer = (char *)malloc((size_t)size + 1u);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "program-registry out of memory reading source for %s\n", metadata->name);
        return 0;
    }
    read_count = fread(buffer, 1u, (size_t)size, file);
    if (read_count != (size_t)size) {
        fprintf(stderr, "program-registry could not read full source for %s\n", metadata->name);
        ok = 0;
    } else {
        buffer[read_count] = '\0';
        *contains = strstr(buffer, needle) != NULL;
    }
    free(buffer);
    fclose(file);
    return ok;
}

static int run_program_registry_probe(void)
{
    DIR *dir;
    struct dirent *entry;
    int archive_files = 0;
    int survival = 0;
    int demo = 0;
    int assignable = 0;
    int ok = 1;

    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        const CazProgramMetadata *metadata = caz_loader_program_metadata_at(index);
        CazCpu cpu;
        CazProgramImage image;
        if (metadata == NULL) {
            fprintf(stderr, "program-registry missing metadata at index %zu\n", index);
            ok = 0;
            continue;
        }
        if (registry_name_count(metadata->name) != 1) {
            fprintf(stderr, "program-registry duplicate name %s\n", metadata->name);
            ok = 0;
        }
        if (metadata->cazenv_assignable && !metadata->survival_participant) {
            fprintf(stderr, "program-registry assigns non-survival program %s to CazEnv\n", metadata->name);
            ok = 0;
        }
        if (metadata->standalone_energy) {
            int contains_include = 0;
            if (!metadata->survival_participant) {
                fprintf(stderr, "program-registry marks non-survival program %s as standalone-energy\n", metadata->name);
                ok = 0;
            }
            if (!program_source_contains(metadata, "survival.inc", &contains_include)) {
                ok = 0;
            } else if (contains_include) {
                fprintf(stderr, "program-registry standalone-energy program %s includes survival.inc\n", metadata->name);
                ok = 0;
            }
        }
        survival += metadata->survival_participant ? 1 : 0;
        demo += metadata->survival_participant ? 0 : 1;
        assignable += metadata->cazenv_assignable ? 1 : 0;
        caz_cpu_init(&cpu, NULL, NULL, NULL);
        if (!caz_loader_load_named(&cpu, metadata->kind, "programs", &image)) {
            fprintf(stderr, "program-registry could not load %s: %s\n", metadata->name, image.error);
            ok = 0;
        } else if (strcmp(image.name, metadata->name) != 0) {
            fprintf(stderr,
                    "program-registry name mismatch for %s: source metadata says %s\n",
                    metadata->name,
                    image.name);
            ok = 0;
        }
    }

    dir = opendir("programs");
    if (dir == NULL) {
        fprintf(stderr, "program-registry could not open programs directory\n");
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char name[CAZ_PROGRAM_NAME_MAX];
        const size_t length = strlen(entry->d_name);
        if (!has_caz_suffix(entry->d_name)) {
            continue;
        }
        archive_files++;
        if (length - 4u >= sizeof(name)) {
            fprintf(stderr, "program-registry archive filename too long: %s\n", entry->d_name);
            ok = 0;
            continue;
        }
        memcpy(name, entry->d_name, length - 4u);
        name[length - 4u] = '\0';
        if (registry_name_count(name) != 1) {
            fprintf(stderr, "program-registry archive file is not registered exactly once: %s\n", entry->d_name);
            ok = 0;
        }
    }
    closedir(dir);

    printf("program-registry total=%zu survival=%d demo=%d assignable=%d path_only=%d archive_files=%d\n",
           caz_loader_program_count(),
           survival,
           demo,
           assignable,
           (int)caz_loader_program_count() - assignable,
           archive_files);
    return ok && archive_files == (int)caz_loader_program_count();
}

static int run_nav_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 7;

    caz_env_init(&state, 0x0ca7e042u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    state.droids[index].x = state.fixtures[0].x - 4.0f;
    state.droids[index].z = state.fixtures[0].z;
    state.droids[index].charge = 0.30f;
    state.droids[index].feral = 0.0f;
    state.droids[index].mode = CAZ_ENV_DROID_PROGRAM;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
    state.droids[index].target_x = state.droids[index].x;
    state.droids[index].target_z = state.droids[index].z;

    for (int step = 0; step < 180; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("nav-probe ");
    print_sample_output(&state, index);
    return state.droids[index].bytecode.output.nav_intent != CAZ_NAV_WANDER &&
           state.droids[index].nav_status != CAZ_NAV_STATUS_IDLE &&
           state.droids[index].nav_status != CAZ_NAV_STATUS_BLOCKED &&
           state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE;
}

static int run_charger_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 7;

    caz_env_init(&state, 0x0ca7e143u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    state.droids[index].x = state.fixtures[0].x - 4.0f;
    state.droids[index].z = state.fixtures[0].z;
    state.droids[index].charge = 0.30f;
    state.droids[index].feral = 0.0f;
    state.droids[index].mode = CAZ_ENV_DROID_PROGRAM;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_NONE;

    for (int step = 0; step < 220; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("charger-probe ");
    print_sample_output(&state, index);
    return state.droids[index].bytecode.output.nav_intent == CAZ_NAV_CHARGER &&
           state.droids[index].mode == CAZ_ENV_DROID_CHARGING &&
           state.droids[index].nav_status == CAZ_NAV_STATUS_DOCKED &&
           state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           state.droids[index].charging_slot < CAZ_ENV_CHARGE_SLOT_COUNT;
}

static int run_full_charger_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 7;

    caz_env_init(&state, 0x0ca7e144u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        state.charge_slots[slot] = (int8_t)slot;
    }
    state.droids[index].x = state.fixtures[0].x + 0.4f;
    state.droids[index].z = state.fixtures[0].z + 0.4f;
    state.droids[index].charge = 0.30f;
    state.droids[index].feral = 0.0f;
    state.droids[index].mode = CAZ_ENV_DROID_PROGRAM;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_NONE;

    for (int step = 0; step < 90; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("full-charger-probe ");
    print_sample_output(&state, index);
    if (state.droids[index].bytecode.output.nav_intent == CAZ_NAV_CHARGER) {
        return state.droids[index].mode != CAZ_ENV_DROID_CHARGING &&
               state.droids[index].nav_status == CAZ_NAV_STATUS_BLOCKED &&
               state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
               state.droids[index].charging_slot == 255u &&
               caz_env_debug_read_port(&state, index, CAZ_PORT_CHARGER_SLOTS) == 0u;
    }
    return state.droids[index].bytecode.output.nav_intent == CAZ_NAV_SOLAR &&
           state.droids[index].mode == CAZ_ENV_DROID_SOLAR_FORAGE &&
           state.droids[index].nav_status == CAZ_NAV_STATUS_SOLAR &&
           state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           state.droids[index].charging_slot == 255u &&
           caz_env_debug_read_port(&state, index, CAZ_PORT_CHARGER_SLOTS) == 0u;
}

static int run_solar_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 7;

    caz_env_init(&state, 0x0ca7e145u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    state.elapsed_seconds = 450.0f;
    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        state.charge_slots[slot] = (int8_t)slot;
    }
    state.droids[index].x = 0.0f;
    state.droids[index].z = 0.0f;
    for (int fixture = 17; fixture < 21; fixture++) {
        state.fixtures[fixture].x = fixture % 2 == 0 ? CAZ_ENV_ROOM_WIDTH_FT * 0.46f : -CAZ_ENV_ROOM_WIDTH_FT * 0.46f;
        state.fixtures[fixture].z = fixture < 19 ? CAZ_ENV_ROOM_LENGTH_FT * 0.46f : -CAZ_ENV_ROOM_LENGTH_FT * 0.46f;
    }
    state.droids[index].charge = 0.08f;
    state.droids[index].feral = 0.0f;
    state.droids[index].mode = CAZ_ENV_DROID_PROGRAM;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
    const float start_charge = state.droids[index].charge;

    for (int step = 0; step < 160; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("solar-probe ");
    print_sample_output(&state, index);
    printf("solar-probe-gains passive=%.6f requested=%.6f charge_delta=%.6f\n",
           state.droids[index].passive_solar_gain,
           state.droids[index].nav_solar_gain,
           state.droids[index].charge - start_charge);
    return state.droids[index].bytecode.output.nav_intent == CAZ_NAV_SOLAR &&
           state.droids[index].nav_status == CAZ_NAV_STATUS_SOLAR &&
           state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           state.droids[index].nav_solar_gain > state.droids[index].passive_solar_gain &&
           state.droids[index].charge > start_charge;
}

static int run_junction_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 7;

    caz_env_init(&state, 0x0ca7e146u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    state.fixtures[17].x = 0.0f;
    state.fixtures[17].z = 0.0f;
    state.droids[index].x = 0.0f;
    state.droids[index].z = 0.0f;
    state.droids[index].charge = 0.20f;
    state.droids[index].feral = 0.0f;
    state.droids[index].mode = CAZ_ENV_DROID_PROGRAM;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
    const float start_charge = state.droids[index].charge;

    for (int step = 0; step < 40; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("junction-probe ");
    print_sample_output(&state, index);
    printf("junction-probe-gains tap=%.6f nav_tap=%.6f fallback_tap=%.6f charge_delta=%.6f distance=%.3f\n",
           state.droids[index].tap_gain,
           state.droids[index].nav_tap_gain,
           state.droids[index].fallback_tap_gain,
           state.droids[index].charge - start_charge,
           nearest_junction_distance_for_droid(&state, &state.droids[index]));
    return state.droids[index].bytecode.output.nav_intent == CAZ_NAV_JUNCTION &&
           state.droids[index].nav_status == CAZ_NAV_STATUS_TAPPING &&
           state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           state.droids[index].energy_source == CAZ_ENV_ENERGY_JUNCTION &&
           state.droids[index].nav_tap_gain > 0.0f &&
           state.droids[index].fallback_tap_gain == 0.0f &&
           state.droids[index].charge > start_charge;
}

static int run_missing_junction_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 7;

    caz_env_init(&state, 0x0ca7e147u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    for (int fixture = 17; fixture < 21; fixture++) {
        state.fixtures[fixture].type = 0u;
    }
    state.droids[index].bytecode.stepping_enabled = 0u;
    state.droids[index].bytecode.output.nav_intent = CAZ_NAV_JUNCTION;
    state.droids[index].mode = CAZ_ENV_DROID_PROGRAM;
    state.droids[index].speed = 0.0f;
    state.droids[index].charge = 0.50f;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_NONE;

    caz_env_step(&state, 0.2f);

    printf("missing-junction-probe ");
    print_sample_output(&state, index);
    return state.droids[index].bytecode.output.nav_intent == CAZ_NAV_JUNCTION &&
           state.droids[index].nav_status == CAZ_NAV_STATUS_BLOCKED &&
           state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_FAILURE &&
           state.droids[index].energy_source != CAZ_ENV_ENERGY_JUNCTION &&
           state.droids[index].tap_gain == 0.0f;
}

static int run_non_bytecode_tap_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 4;

    caz_env_init(&state, 0x0ca7e148u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    state.droids[index].x = state.fixtures[17].x;
    state.droids[index].z = state.fixtures[17].z;
    state.droids[index].charge = 0.50f;
    state.droids[index].mode = CAZ_ENV_DROID_TAP_JUNCTION;
    state.droids[index].speed = 1.0f;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_SUPERVISOR;
    state.droids[index].bytecode.stepping_enabled = 0u;
    state.droids[index].bytecode.output.nav_intent = CAZ_NAV_WANDER;

    for (int step = 0; step < 8; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("non-bytecode-tap-probe ");
    print_sample_output(&state, index);
    printf("non-bytecode-tap-gains tap=%.6f nav_tap=%.6f fallback_tap=%.6f energy=%u\n",
           state.droids[index].tap_gain,
           state.droids[index].nav_tap_gain,
           state.droids[index].fallback_tap_gain,
           state.droids[index].energy_source);
    return state.droids[index].bytecode.output.nav_intent == CAZ_NAV_WANDER &&
           state.droids[index].nav_status == CAZ_NAV_STATUS_BLOCKED &&
           state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_SUPERVISOR &&
           state.droids[index].energy_source != CAZ_ENV_ENERGY_JUNCTION &&
           state.droids[index].tap_gain == 0.0f &&
           state.droids[index].nav_tap_gain == 0.0f &&
           state.droids[index].fallback_tap_gain == 0.0f;
}

static int run_obstacle_probe(void)
{
    CazEnvState state;
    CazEnvDroid *droid;
    char load_error[512];
    const int index = 0;
    const CazEnvFixture *fixture;
    float radius;
    uint8_t terrain;
    uint8_t edge;

    caz_env_init(&state, 0x0ca7e149u);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    droid = &state.droids[index];
    fixture = &state.fixtures[5];
    radius = (fixture->width + fixture->length) * 0.25f + 0.52f;
    droid->x = fixture->x - radius;
    droid->z = fixture->z;
    droid->yaw = 1.5707963f;
    droid->charge = 0.80f;
    droid->mode = CAZ_ENV_DROID_PROGRAM;
    droid->nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
    droid->bytecode.stepping_enabled = 0u;
    droid->bytecode.output.nav_intent = CAZ_NAV_WANDER;
    droid->bytecode.output.gait = 1u;
    droid->bytecode.output.skill = CAZ_BODY_SKILL_WALK;
    droid->bytecode.output.head_yaw = 128u;

    for (int step = 0; step < 6; step++) {
        caz_env_step(&state, 0.2f);
    }

    terrain = caz_env_debug_read_port(&state, index, CAZ_PORT_TERRAIN);
    edge = caz_env_debug_read_port(&state, index, CAZ_PORT_EYE_EDGE);
    printf("obstacle-probe blocked=%u obstacle=%u terrain=%u edge=%u nav_status=%u pos=%.2f/%.2f fixture=%u\n",
           droid->blocked_movement_count,
           droid->obstacle_state,
           terrain,
           edge,
           droid->nav_status,
           droid->x,
           droid->z,
           fixture->type);

    return droid->blocked_movement_count > 0u &&
           droid->obstacle_state != 0u &&
           terrain >= 4u &&
           edge >= 200u &&
           droid->nav_status == CAZ_NAV_STATUS_BLOCKED;
}

static int run_strategy_probe(void)
{
    CazEnvState state;
    CazEnvDroid *house;
    CazEnvDroid *feral;
    char load_error[512];
    const int house_index = 8;
    const int feral_index = 9;

    caz_env_init(&state, 0x0ca7e14au);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    if (!caz_env_assign_program(&state, house_index, CAZ_PROGRAM_CURIOUS_PATROL, "programs", load_error, sizeof(load_error)) ||
        !caz_env_assign_program(&state, feral_index, CAZ_PROGRAM_CURIOUS_PATROL, "programs", load_error, sizeof(load_error))) {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    state.elapsed_seconds = 450.0f;
    fill_charger_for_probe(&state, 0.70f);
    for (int fixture = 17; fixture < 21; fixture++) {
        state.fixtures[fixture].x = fixture % 2 == 0 ? CAZ_ENV_ROOM_WIDTH_FT * 0.46f : -CAZ_ENV_ROOM_WIDTH_FT * 0.46f;
        state.fixtures[fixture].z = fixture < 19 ? CAZ_ENV_ROOM_LENGTH_FT * 0.46f : -CAZ_ENV_ROOM_LENGTH_FT * 0.46f;
    }

    house = &state.droids[house_index];
    feral = &state.droids[feral_index];
    park_probe_droid(&state, house_index, state.fixtures[0].x + 1.2f, state.fixtures[0].z);
    park_probe_droid(&state, feral_index, state.fixtures[0].x + 4.0f, state.fixtures[0].z + 4.0f);
    house->charge = 0.30f;
    house->feral = 0.10f;
    feral->charge = 0.30f;
    feral->feral = 0.90f;

    for (int step = 0; step < 160; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("strategy-probe house(nav=%u status=%u strategy=%u skill=%u gait=%u charge=%.3f) feral(nav=%u status=%u strategy=%u skill=%u gait=%u charge=%.3f)\n",
           house->bytecode.output.nav_intent,
           house->nav_status,
           caz_env_debug_read_port(&state, house_index, CAZ_PORT_STRATEGY_TENDENCY),
           house->bytecode.output.skill,
           house->bytecode.output.gait,
           house->charge,
           feral->bytecode.output.nav_intent,
           feral->nav_status,
           caz_env_debug_read_port(&state, feral_index, CAZ_PORT_STRATEGY_TENDENCY),
           feral->bytecode.output.skill,
           feral->bytecode.output.gait,
           feral->charge);

    return house->bytecode.output.nav_intent == CAZ_NAV_CHARGER &&
           house->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           house->bytecode.output.skill == CAZ_BODY_SKILL_REST &&
           house->bytecode.output.gait == 0u &&
           house->charge > 0.0f &&
           feral->bytecode.output.nav_intent == CAZ_NAV_SOLAR &&
           feral->nav_status == CAZ_NAV_STATUS_SOLAR &&
           feral->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           feral->charge > 0.0f;
}

static int run_charger_queue_soak_probe(void)
{
    CazEnvState state;
    CazEnvDroid *waiter;
    char load_error[512];
    const int waiter_index = 8;
    int acquired_step = -1;
    int alternate_count = 0;
    int invalid_slot_state = 0;
    int gain_while_full = 0;
    float waiter_gain_at_full = 0.0f;
    float minimum_waiter_charge = 1.0f;

    caz_env_init(&state, 0x0ca7e14bu);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    if (!caz_env_assign_program(&state, waiter_index, CAZ_PROGRAM_CURIOUS_PATROL, "programs", load_error, sizeof(load_error))) {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    state.elapsed_seconds = 450.0f;
    fill_charger_for_probe(&state, 0.979f);
    waiter = &state.droids[waiter_index];
    park_probe_droid(&state, waiter_index, state.fixtures[0].x + 1.15f, state.fixtures[0].z);
    waiter->charge = 0.30f;
    waiter->feral = 0.10f;

    for (int index = waiter_index + 1; index < CAZ_ENV_DROID_COUNT; index++) {
        CazEnvDroid *droid = &state.droids[index];
        park_probe_droid(&state,
                         index,
                         state.fixtures[0].x + 5.0f + (float)(index - waiter_index),
                         state.fixtures[0].z + 6.0f);
        droid->charge = 0.30f;
        droid->feral = 0.90f;
    }

    for (int step = 0; step < 900; step++) {
        const uint8_t free_before = caz_env_debug_read_port(&state, waiter_index, CAZ_PORT_CHARGER_SLOTS);
        const float waiter_gain_before = waiter->charger_gain;
        caz_env_step(&state, 0.2f);
        if (!charge_slots_valid(&state)) {
            invalid_slot_state = 1;
        }
        if (free_before == 0u &&
            waiter->charging_slot == 255u &&
            waiter->charger_gain > waiter_gain_before + 0.0000001f) {
            gain_while_full = 1;
        }
        if (free_before == 0u) {
            waiter_gain_at_full = waiter->charger_gain;
        }
        if (waiter->charge < minimum_waiter_charge) {
            minimum_waiter_charge = waiter->charge;
        }
        for (int index = waiter_index + 1; index < CAZ_ENV_DROID_COUNT; index++) {
            const uint8_t nav = state.droids[index].bytecode.output.nav_intent;
            if (nav == CAZ_NAV_SOLAR || nav == CAZ_NAV_JUNCTION) {
                alternate_count++;
                break;
            }
        }
        if (acquired_step < 0 &&
            waiter->mode == CAZ_ENV_DROID_CHARGING &&
            waiter->charging_slot < CAZ_ENV_CHARGE_SLOT_COUNT &&
            state.charge_slots[waiter->charging_slot] == waiter_index) {
            acquired_step = step;
        }
    }

    printf("charger-queue-probe acquired_step=%d waiter_slot=%u waiter_charge=%.3f min_waiter=%.3f charger_gain=%.6f gain_at_full=%.6f alternates=%d invalid_slots=%d gain_while_full=%d free_slots=%u\n",
           acquired_step,
           waiter->charging_slot,
           waiter->charge,
           minimum_waiter_charge,
           waiter->charger_gain,
           waiter_gain_at_full,
           alternate_count,
           invalid_slot_state,
           gain_while_full,
           caz_env_debug_read_port(&state, waiter_index, CAZ_PORT_CHARGER_SLOTS));

    return acquired_step >= 0 &&
           waiter->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           waiter->bytecode.output.nav_intent == CAZ_NAV_CHARGER &&
           waiter->charger_gain > waiter_gain_at_full &&
           waiter->charge > minimum_waiter_charge &&
           alternate_count > 0 &&
           !invalid_slot_state &&
           !gain_while_full &&
           minimum_waiter_charge > 0.0f;
}

static int run_low_sun_solar_accounting_probe(void)
{
    CazEnvState state;
    CazEnvDroid *droid;
    char load_error[512];
    const int index = 8;
    float start_charge;

    caz_env_init(&state, 0x0ca7e14cu);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }

    state.elapsed_seconds = 1350.0f;
    fill_charger_for_probe(&state, 0.70f);
    for (int fixture = 17; fixture < 21; fixture++) {
        state.fixtures[fixture].x = fixture % 2 == 0 ? CAZ_ENV_ROOM_WIDTH_FT * 0.46f : -CAZ_ENV_ROOM_WIDTH_FT * 0.46f;
        state.fixtures[fixture].z = fixture < 19 ? CAZ_ENV_ROOM_LENGTH_FT * 0.46f : -CAZ_ENV_ROOM_LENGTH_FT * 0.46f;
    }
    droid = &state.droids[index];
    park_probe_droid(&state, index, 0.0f, 0.0f);
    droid->charge = 0.08f;
    droid->feral = 0.90f;
    start_charge = droid->charge;

    for (int step = 0; step < 180; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("low-sun-solar-probe nav=%u status=%u cause=%u solar_level=%u passive=%.6f requested=%.6f fallback=%.6f charge_delta=%.6f\n",
           droid->bytecode.output.nav_intent,
           droid->nav_status,
           droid->nav_cause,
           caz_env_debug_read_port(&state, index, CAZ_PORT_SOLAR_LEVEL),
           droid->passive_solar_gain,
           droid->nav_solar_gain,
           droid->fallback_solar_gain,
           droid->charge - start_charge);

    return droid->bytecode.output.nav_intent == CAZ_NAV_SOLAR &&
           droid->nav_status == CAZ_NAV_STATUS_SOLAR &&
           droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
           droid->nav_solar_gain > droid->passive_solar_gain &&
           droid->fallback_solar_gain == 0.0f &&
           droid->charge > start_charge;
}

static int run_non_bytecode_solar_probe(void)
{
    CazEnvState state;
    CazEnvDroid *droid;
    char load_error[512];
    const int index = 4;
    const float start_charge = 0.50f;

    caz_env_init(&state, 0x0ca7e14du);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    droid = &state.droids[index];
    droid->charge = start_charge;
    droid->mode = CAZ_ENV_DROID_SOLAR_FORAGE;
    droid->speed = 0.0f;
    droid->nav_cause = CAZ_ENV_NAV_CAUSE_SUPERVISOR;
    droid->bytecode.stepping_enabled = 0u;
    droid->bytecode.output.nav_intent = CAZ_NAV_WANDER;

    for (int step = 0; step < 12; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("non-bytecode-solar-probe nav=%u status=%u cause=%u passive=%.6f requested=%.6f fallback=%.6f charge_delta=%.6f\n",
           droid->bytecode.output.nav_intent,
           droid->nav_status,
           droid->nav_cause,
           droid->passive_solar_gain,
           droid->nav_solar_gain,
           droid->fallback_solar_gain,
           droid->charge - start_charge);

    return droid->bytecode.output.nav_intent == CAZ_NAV_WANDER &&
           droid->nav_solar_gain == 0.0f &&
           droid->fallback_solar_gain == 0.0f;
}

static int run_non_bytecode_charger_probe(void)
{
    CazEnvState state;
    CazEnvDroid *droid;
    char load_error[512];
    const int index = 4;
    const float start_charge = 0.50f;

    caz_env_init(&state, 0x0ca7e14eu);
    if (!load_probe_programs(&state, load_error, sizeof(load_error))) {
        return 0;
    }
    isolate_probe_droid(&state, index);

    droid = &state.droids[index];
    droid->charge = start_charge;
    droid->mode = CAZ_ENV_DROID_CHARGING;
    droid->charging_slot = 0u;
    state.charge_slots[0] = (int8_t)index;
    droid->energy_source = CAZ_ENV_ENERGY_CHARGER;
    droid->nav_cause = CAZ_ENV_NAV_CAUSE_SUPERVISOR;
    droid->bytecode.stepping_enabled = 0u;
    droid->bytecode.output.nav_intent = CAZ_NAV_WANDER;

    caz_env_step(&state, 0.2f);

    printf("non-bytecode-charger-probe nav=%u status=%u cause=%u mode=%u slot=%u charger_gain=%.6f charge_delta=%.6f\n",
           droid->bytecode.output.nav_intent,
           droid->nav_status,
           droid->nav_cause,
           droid->mode,
           droid->charging_slot,
           droid->charger_gain,
           droid->charge - start_charge);

    return droid->charger_gain == 0.0f &&
           droid->charging_slot == 255u &&
           droid->mode != CAZ_ENV_DROID_CHARGING &&
           droid->nav_cause == CAZ_ENV_NAV_CAUSE_FAILURE;
}

static void move_junctions_far(CazEnvState *state)
{
    for (int fixture = 17; fixture < 21; fixture++) {
        state->fixtures[fixture].x = fixture % 2 == 0 ? CAZ_ENV_ROOM_WIDTH_FT * 0.46f : -CAZ_ENV_ROOM_WIDTH_FT * 0.46f;
        state->fixtures[fixture].z = fixture < 19 ? CAZ_ENV_ROOM_LENGTH_FT * 0.46f : -CAZ_ENV_ROOM_LENGTH_FT * 0.46f;
    }
}

static void place_primary_junction(CazEnvState *state, float x, float z)
{
    move_junctions_far(state);
    state->fixtures[17].type = CAZ_ENV_FIXTURE_JUNCTION_BOX;
    state->fixtures[17].x = x;
    state->fixtures[17].z = z;
    state->fixtures[17].y = 0.5f;
}

static void clear_a10_probe_path(CazEnvState *state)
{
    for (int fixture = 0; fixture < 17; fixture++) {
        state->fixtures[fixture].x = -12.0f + (float)(fixture % 5) * 6.0f;
        state->fixtures[fixture].z = fixture < 9 ? -24.0f : 24.0f;
    }
}

static void prepare_stress_droid(CazEnvState *state,
                                 const CazProgramMetadata *metadata,
                                 int index,
                                 float x,
                                 float z,
                                 float charge,
                                 float feral,
                                 char *load_error,
                                 size_t load_error_length)
{
    if (!caz_env_assign_program(state, index, metadata->kind, "programs", load_error, load_error_length)) {
        return;
    }
    isolate_probe_droid(state, index);
    park_probe_droid(state, index, x, z);
    state->droids[index].charge = charge;
    state->droids[index].feral = feral;
    state->droids[index].bytecode.output.gait = 0u;
    state->droids[index].bytecode.output.skill = CAZ_BODY_SKILL_REST;
    state->droids[index].speed = 0.0f;
}

static int stress_result_line(const CazProgramMetadata *metadata,
                              const char *scenario,
                              const char *expected,
                              const CazEnvState *state,
                              int index,
                              float start_charge,
                              int ok)
{
    const CazEnvDroid *droid = &state->droids[index];
    const float junction_distance = nearest_junction_distance_for_droid(state, droid);
    printf("stress-program name=%s scenario=%s expected=%s actual=(nav=%u status=%u cause=%u mode=%s charge=%.4f delta=%.4f charger=%.6f solar=%.6f tap=%.6f junction=%.2f obstacle=%u blocked=%u junction_blocked=%u) result=%s\n",
           metadata->name,
           scenario,
           expected,
           droid->bytecode.output.nav_intent,
           droid->nav_status,
           droid->nav_cause,
           mode_name(droid->mode),
           droid->charge,
           droid->charge - start_charge,
           droid->charger_gain,
           droid->nav_solar_gain,
           droid->nav_tap_gain,
           junction_distance,
           droid->obstacle_state,
           droid->blocked_movement_count,
           droid->blocked_junction_count,
           ok ? "PASS" : "FAIL");
    return ok;
}

static int run_stress_open_charger(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.30f;

    caz_env_init(&state, 0x0ca7e400u + (uint32_t)metadata->kind);
    move_junctions_far(&state);
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         state.fixtures[0].x - 4.0f,
                         state.fixtures[0].z,
                         start_charge,
                         0.10f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    for (int step = 0; step < 260; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "open-charger",
                              "bytecode NAV_CHARGER dock/gain",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].bytecode.output.nav_intent == CAZ_NAV_CHARGER &&
                                  state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  state.droids[index].charger_gain > 0.0f &&
                                  state.droids[index].charge > start_charge);
}

static int run_stress_full_charger(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.30f;

    caz_env_init(&state, 0x0ca7e500u + (uint32_t)metadata->kind);
    fill_charger_for_probe(&state, 0.70f);
    state.elapsed_seconds = 450.0f;
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         state.fixtures[0].x + 1.2f,
                         state.fixtures[0].z,
                         start_charge,
                         0.10f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    for (int step = 0; step < 140; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "full-charger",
                              "bytecode wait or alternate recovery, no zero charge",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  state.droids[index].charge > 0.0f &&
                                  state.droids[index].charging_slot == 255u &&
                                  (state.droids[index].bytecode.output.nav_intent == CAZ_NAV_CHARGER ||
                                   state.droids[index].bytecode.output.nav_intent == CAZ_NAV_SOLAR ||
                                   state.droids[index].bytecode.output.nav_intent == CAZ_NAV_JUNCTION));
}

static int run_stress_critical_junction(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.08f;

    caz_env_init(&state, 0x0ca7e600u + (uint32_t)metadata->kind);
    place_primary_junction(&state, 0.0f, 0.0f);
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         0.45f,
                         0.0f,
                         start_charge,
                         0.35f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    for (int step = 0; step < 100; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "critical-junction",
                              "bytecode NAV_JUNCTION tap gain",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].bytecode.output.nav_intent == CAZ_NAV_JUNCTION &&
                                  state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  state.droids[index].nav_tap_gain > 0.0f &&
                                  state.droids[index].fallback_tap_gain == 0.0f &&
                                  state.droids[index].charge > start_charge);
}

static int run_stress_low_sun(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.08f;

    caz_env_init(&state, 0x0ca7e700u + (uint32_t)metadata->kind);
    state.elapsed_seconds = 1350.0f;
    fill_charger_for_probe(&state, 0.70f);
    move_junctions_far(&state);
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         0.0f,
                         0.0f,
                         start_charge,
                         0.90f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    for (int step = 0; step < 180; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "low-sun",
                              "bytecode NAV_SOLAR positive requested gain",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].bytecode.output.nav_intent == CAZ_NAV_SOLAR &&
                                  state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  state.droids[index].nav_solar_gain > state.droids[index].passive_solar_gain &&
                                  state.droids[index].fallback_solar_gain == 0.0f &&
                                  state.droids[index].charge > start_charge);
}

static int run_stress_high_sun(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.30f;

    caz_env_init(&state, 0x0ca7e800u + (uint32_t)metadata->kind);
    state.elapsed_seconds = 450.0f;
    fill_charger_for_probe(&state, 0.70f);
    move_junctions_far(&state);
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         0.0f,
                         0.0f,
                         start_charge,
                         0.90f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    for (int step = 0; step < 120; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "high-sun",
                              "feral bytecode NAV_SOLAR",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].bytecode.output.nav_intent == CAZ_NAV_SOLAR &&
                                  state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  state.droids[index].nav_solar_gain > 0.0f &&
                                  state.droids[index].charge > start_charge);
}

static int run_stress_obstructed_route(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    CazEnvFixture *bed;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.30f;

    caz_env_init(&state, 0x0ca7e900u + (uint32_t)metadata->kind);
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         state.fixtures[0].x - 3.4f,
                         state.fixtures[0].z,
                         start_charge,
                         0.10f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }
    bed = &state.fixtures[5];
    bed->type = CAZ_ENV_FIXTURE_CAT_BED;
    bed->x = state.fixtures[0].x - 2.2f;
    bed->z = state.fixtures[0].z;
    bed->width = 2.0f;
    bed->length = 1.65f;

    for (int step = 0; step < 80; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "obstructed-route",
                              "recovery intent stays bytecode-owned while blocked",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].bytecode.output.nav_intent == CAZ_NAV_CHARGER &&
                                  state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  (state.droids[index].nav_status == CAZ_NAV_STATUS_BLOCKED ||
                                   state.droids[index].charger_gain > 0.0f) &&
                                  (state.droids[index].blocked_movement_count > 0u ||
                                   state.droids[index].charger_gain > 0.0f) &&
                                  state.droids[index].charge > 0.0f);
}

static int run_stress_a10_junction_reach(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.020f;

    caz_env_init(&state, 0x0ca7ea00u + (uint32_t)metadata->kind);
    place_primary_junction(&state, 0.0f, 0.0f);
    clear_a10_probe_path(&state);
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         1.54f,
                         0.0f,
                         start_charge,
                         0.20f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    for (int step = 0; step < 320; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "a10-junction-reach",
                              "near blocked NAV_JUNCTION gains tap before zero",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].bytecode.output.nav_intent == CAZ_NAV_JUNCTION &&
                                  state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  state.droids[index].nav_tap_gain > 0.0f &&
                                  state.droids[index].fallback_tap_gain == 0.0f &&
                                  state.droids[index].charge > 0.0f);
}

static int run_stress_a11_route_energy(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    char load_error[512];
    const int index = 8;
    const float start_charge = 0.045f;

    caz_env_init(&state, 0x0ca7eb00u + (uint32_t)metadata->kind);
    move_junctions_far(&state);
    for (int fixture = 17; fixture < 21; fixture++) {
        state.fixtures[fixture].x = CAZ_ENV_ROOM_WIDTH_FT * 0.46f;
        state.fixtures[fixture].z = CAZ_ENV_ROOM_LENGTH_FT * 0.46f;
    }
    state.fixtures[0].x = CAZ_ENV_ROOM_WIDTH_FT * 0.42f;
    state.fixtures[0].z = CAZ_ENV_ROOM_LENGTH_FT * 0.42f;
    state.elapsed_seconds = 450.0f;
    prepare_stress_droid(&state,
                         metadata,
                         index,
                         -CAZ_ENV_ROOM_WIDTH_FT * 0.42f,
                         -CAZ_ENV_ROOM_LENGTH_FT * 0.42f,
                         start_charge,
                         0.20f,
                         load_error,
                         sizeof(load_error));
    if (load_error[0] != '\0') {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    for (int step = 0; step < 220; step++) {
        caz_env_step(&state, 0.2f);
    }

    return stress_result_line(metadata,
                              "a11-route-energy",
                              "bytecode NAV_SOLAR when charger/junction are infeasible",
                              &state,
                              index,
                              start_charge,
                              state.droids[index].bytecode.output.nav_intent == CAZ_NAV_SOLAR &&
                                  state.droids[index].nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                  state.droids[index].nav_solar_gain > 0.0f &&
                                  state.droids[index].charger_gain == 0.0f &&
                                  state.droids[index].nav_tap_gain == 0.0f &&
                                  state.droids[index].fallback_tap_gain == 0.0f &&
                                  state.droids[index].charge > start_charge);
}

static int run_stress_program_matrix(const CazProgramMetadata *metadata)
{
    int ok = 1;
    ok &= run_stress_open_charger(metadata);
    ok &= run_stress_full_charger(metadata);
    ok &= run_stress_critical_junction(metadata);
    ok &= run_stress_low_sun(metadata);
    ok &= run_stress_high_sun(metadata);
    ok &= run_stress_obstructed_route(metadata);
    ok &= run_stress_a10_junction_reach(metadata);
    ok &= run_stress_a11_route_energy(metadata);
    return ok;
}

static int run_stress_suite(void)
{
    int ok = 1;
    int survival_programs = 0;
    int demo_programs = 0;

    if (!run_program_registry_probe()) {
        return 0;
    }

    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        const CazProgramMetadata *metadata = caz_loader_program_metadata_at(index);
        if (metadata == NULL) {
            ok = 0;
            continue;
        }
        if (!metadata->survival_participant) {
            demo_programs++;
            printf("stress-program name=%s scenario=excluded-demo expected=not survival participant actual=(class=demo) result=PASS\n",
                   metadata->name);
            continue;
        }
        survival_programs++;
        if (!run_stress_program_matrix(metadata)) {
            ok = 0;
        }
    }

    printf("stress-result=%s survival_programs=%d scenarios_per_program=8 excluded_demo=%d\n",
           ok ? "PASS" : "FAIL",
           survival_programs,
           demo_programs);
    return ok;
}

static int run_forced_low_program_probe(const CazProgramMetadata *metadata)
{
    CazEnvState state;
    CazEnvDroid *droid;
    char load_error[512];
    int ok;

    caz_env_init(&state, 0x0ca7e250u + (uint32_t)metadata->kind);
    if (!caz_env_assign_program(&state, 0, metadata->kind, "programs", load_error, sizeof(load_error))) {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }

    droid = &state.droids[0];
    droid->charge = 0.18f;
    droid->feral = 0.0f;
    droid->mode = CAZ_ENV_DROID_PROGRAM;
    droid->nav_cause = CAZ_ENV_NAV_CAUSE_NONE;
    droid->nav_status = CAZ_NAV_STATUS_IDLE;
    droid->energy_source = CAZ_ENV_ENERGY_BATTERY;
    droid->target_x = droid->x;
    droid->target_z = droid->z;

    for (int step = 0; step < 260; step++) {
        caz_env_step(&state, 0.2f);
    }

    ok = droid->bytecode.image_loaded &&
         droid->bytecode.stepping_enabled &&
         !droid->bytecode.faulted &&
         !droid->bytecode.cpu.halted &&
         (droid->bytecode.output.nav_intent == CAZ_NAV_CHARGER ||
          droid->bytecode.output.nav_intent == CAZ_NAV_SOLAR ||
          droid->bytecode.output.nav_intent == CAZ_NAV_JUNCTION) &&
         droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE;

    printf("regression-program name=%s class=%s nav=%u status=%u cause=%u fault=%u halted=%u instructions=%llu result=%s\n",
           metadata->name,
           metadata->survival_participant ? "survival" : "demo",
           droid->bytecode.output.nav_intent,
           droid->nav_status,
           droid->nav_cause,
           droid->bytecode.faulted,
           droid->bytecode.cpu.halted ? 1u : 0u,
           (unsigned long long)droid->bytecode.cpu.instructions,
           ok ? "PASS" : "FAIL");
    return ok;
}

static int run_cli_program_probe(const CazProgramMetadata *metadata)
{
    CazDroid droid;
    CazCpu cpu;
    CazProgramImage image;
    int faulted = 0;

    caz_droid_init(&droid, CAZ_SCENARIO_FARMYARD, 0x0ca70000u + (uint32_t)metadata->kind);
    caz_cpu_init(&cpu, caz_droid_read_port, caz_droid_write_port, &droid);
    if (!caz_loader_load_named(&cpu, metadata->kind, "programs", &image)) {
        fprintf(stderr, "regression-cli failed to load %s: %s\n", metadata->name, image.error);
        return 0;
    }

    for (int tick = 0; tick < 24 && !cpu.halted && !faulted; tick++) {
        caz_droid_tick(&droid);
        for (int instruction = 0; instruction < 24 && !cpu.halted; instruction++) {
            if (caz_cpu_step(&cpu) < 0) {
                faulted = 1;
                break;
            }
        }
    }

    printf("regression-cli name=%s fault=%d halted=%u instructions=%llu nav=%u/%u result=%s\n",
           metadata->name,
           faulted,
           cpu.halted ? 1u : 0u,
           (unsigned long long)cpu.instructions,
           droid.nav_intent,
           droid.nav_status,
           faulted ? "FAIL" : "PASS");
    return !faulted;
}

static int run_regression_suite(void)
{
    int ok = 1;
    int programs_checked = 0;

    if (!run_program_registry_probe()) {
        return 0;
    }

    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        const CazProgramMetadata *metadata = caz_loader_program_metadata_at(index);
        if (metadata == NULL) {
            ok = 0;
            continue;
        }
        programs_checked++;
        if (!run_cli_program_probe(metadata)) {
            ok = 0;
        }
        if (!run_forced_low_program_probe(metadata)) {
            ok = 0;
        }
    }

    printf("regression-result=%s programs=%d\n", ok ? "PASS" : "FAIL", programs_checked);
    return ok;
}

static int run_all_probes(void)
{
    if (!run_program_registry_probe()) {
        fprintf(stderr, "program-registry-probe failed: programs/ archive and loader registry differ\n");
        return 0;
    }
    if (!run_nav_probe()) {
        fprintf(stderr, "nav-probe failed: bytecode survival prologue did not request a usable recovery navigation mode\n");
        return 0;
    }
    if (!run_charger_probe()) {
        fprintf(stderr, "charger-probe failed: bytecode NAV_CHARGER did not dock and charge\n");
        return 0;
    }
    if (!run_full_charger_probe()) {
        fprintf(stderr, "full-charger-probe failed: full charger did not force bytecode-owned blocked loiter or alternate recovery\n");
        return 0;
    }
    if (!run_solar_probe()) {
        fprintf(stderr, "solar-probe failed: bytecode NAV_SOLAR did not produce requested solar recovery\n");
        return 0;
    }
    if (!run_junction_probe()) {
        fprintf(stderr, "junction-probe failed: bytecode NAV_JUNCTION did not produce requested tap recovery\n");
        return 0;
    }
    if (!run_missing_junction_probe()) {
        fprintf(stderr, "missing-junction-probe failed: unavailable junction did not report blocked failure\n");
        return 0;
    }
    if (!run_non_bytecode_tap_probe()) {
        fprintf(stderr, "non-bytecode-tap-probe failed: fallback tap mode received untracked junction energy\n");
        return 0;
    }
    if (!run_obstacle_probe()) {
        fprintf(stderr, "obstacle-probe failed: blocked movement did not report through obstacle, terrain, and eye edge\n");
        return 0;
    }
    if (!run_strategy_probe()) {
        fprintf(stderr, "strategy-probe failed: house and feral bytecode recovery choices did not diverge under pressure\n");
        return 0;
    }
    if (!run_charger_queue_soak_probe()) {
        fprintf(stderr, "charger-queue-probe failed: full charger queue did not release and reassign a bytecode-owned slot safely\n");
        return 0;
    }
    if (!run_low_sun_solar_accounting_probe()) {
        fprintf(stderr, "low-sun-solar-probe failed: low-sun requested solar gain was not attributed to bytecode NAV_SOLAR\n");
        return 0;
    }
    if (!run_non_bytecode_solar_probe()) {
        fprintf(stderr, "non-bytecode-solar-probe failed: non-bytecode solar mode received requested or fallback solar gain\n");
        return 0;
    }
    if (!run_non_bytecode_charger_probe()) {
        fprintf(stderr, "non-bytecode-charger-probe failed: non-bytecode charging received charger gain or retained a slot\n");
        return 0;
    }
    printf("probe-result=PASS\n");
    return 1;
}

static void init_track(DroidTrack *track)
{
    memset(track, 0, sizeof(*track));
    track->minimum_charge = FLT_MAX;
}

static void apply_matrix_case(CazEnvState *state, MatrixCase matrix_case)
{
    if (state == NULL) {
        return;
    }

    switch (matrix_case) {
    case MATRIX_CASE_LOW_SUN:
        state->elapsed_seconds = 1350.0f;
        break;
    case MATRIX_CASE_FULL_CHARGER:
        fill_charger_for_probe(state, 0.70f);
        break;
    case MATRIX_CASE_DISTANT_JUNCTION:
        move_junctions_far(state);
        break;
    case MATRIX_CASE_HIGH_OBSTACLE:
        for (int fixture = 5; fixture < 17; fixture++) {
            CazEnvFixture *bed = &state->fixtures[fixture];
            bed->type = CAZ_ENV_FIXTURE_CAT_BED;
            bed->x = state->fixtures[0].x + ((float)((fixture - 5) % 4) - 1.5f) * 1.35f;
            bed->z = state->fixtures[0].z + 2.2f + (float)((fixture - 5) / 4) * 1.25f;
            bed->width = 2.0f;
            bed->length = 1.65f;
            bed->height = 0.32f;
        }
        break;
    case MATRIX_CASE_BASELINE:
    case MATRIX_CASE_COUNT:
    default:
        break;
    }
}

static int run_survival(const HarnessOptions *options)
{
    const float dt_seconds = 0.2f;
    const long steps_per_day = (long)(24.0f * 60.0f * 60.0f / dt_seconds);
    const long total_steps = steps_per_day * (long)options->days;
    const int strict = is_strict_mode(options->mode);
    int failed_seeds = 0;
    FailureReport first_failure;

    memset(&first_failure, 0, sizeof(first_failure));

    for (int seed_index = 0; seed_index < options->seed_count; seed_index++) {
        CazEnvState state;
        DroidTrack tracks[CAZ_ENV_DROID_COUNT];
        const uint32_t seed = 0x0ca7e000u + (uint32_t)seed_index;
        char load_error[512];

        caz_env_init(&state, seed);
        if (!caz_env_load_programs(&state, "programs", load_error, sizeof(load_error))) {
            fprintf(stderr, "%s\n", load_error);
            return 1;
        }
        apply_matrix_case(&state, options->matrix_case);
        if (seed_index == 0) {
            printf("matrix-case=%s\n", matrix_case_name(options->matrix_case));
            count_fixtures(&state);
            count_bytecode_runtimes(&state);
            print_sample_ports(&state);
        }

        for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
            init_track(&tracks[index]);
        }

        for (long step = 0; step < total_steps; step++) {
            caz_env_step(&state, dt_seconds);
            for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
                const CazEnvDroid *droid = &state.droids[index];
                if (droid->charge < tracks[index].minimum_charge) {
                    tracks[index].minimum_charge = droid->charge;
                }
                tracks[index].ever_charging |= droid->mode == CAZ_ENV_DROID_CHARGING;
                tracks[index].ever_depleted |= droid->mode == CAZ_ENV_DROID_DEPLETED;
                tracks[index].ever_returning |= droid->mode == CAZ_ENV_DROID_RETURN_TO_CHARGE;
                tracks[index].ever_solar |= droid->mode == CAZ_ENV_DROID_SOLAR_FORAGE ||
                                             droid->energy_source == CAZ_ENV_ENERGY_SOLAR;
                tracks[index].ever_tapping |= droid->mode == CAZ_ENV_DROID_TAP_JUNCTION ||
                                              droid->energy_source == CAZ_ENV_ENERGY_JUNCTION;
                tracks[index].ever_zero_charge |= droid->charge <= 0.000001f;
                tracks[index].ever_nav_charger |= droid->bytecode.output.nav_intent == CAZ_NAV_CHARGER;
                tracks[index].ever_nav_solar |= droid->bytecode.output.nav_intent == CAZ_NAV_SOLAR;
                tracks[index].ever_nav_junction |= droid->bytecode.output.nav_intent == CAZ_NAV_JUNCTION;
                tracks[index].ever_bytecode_recovery |= droid->nav_cause == CAZ_ENV_NAV_CAUSE_BYTECODE &&
                                                         droid->bytecode.output.nav_intent != CAZ_NAV_WANDER;
                tracks[index].ever_supervisor_recovery |= droid->nav_cause == CAZ_ENV_NAV_CAUSE_SUPERVISOR;
                record_recovery_trace(&tracks[index], step, droid);
                if (!first_failure.captured &&
                    (droid->charge <= 0.000001f || droid->mode == CAZ_ENV_DROID_DEPLETED)) {
                    capture_failure(&first_failure, &state, &tracks[index], seed, step, index);
                }
            }
        }

        int final_modes[7] = {0};
        int ever_charging = 0;
        int ever_depleted = 0;
        int ever_returning = 0;
        int ever_solar = 0;
        int ever_tapping = 0;
        int ever_zero_charge = 0;
        int ever_nav_charger = 0;
        int ever_nav_solar = 0;
        int ever_nav_junction = 0;
        int ever_bytecode_recovery = 0;
        int ever_supervisor_recovery = 0;
        int final_zero_charge = 0;
        int blocked_droids = 0;
        float final_minimum_charge = FLT_MAX;
        float observed_minimum_charge = FLT_MAX;
        float final_charge_sum = 0.0f;
        float charger_gain_sum = 0.0f;
        float solar_gain_sum = 0.0f;
        float passive_solar_gain_sum = 0.0f;
        float nav_solar_gain_sum = 0.0f;
        float fallback_solar_gain_sum = 0.0f;
        float tap_gain_sum = 0.0f;
        float nav_tap_gain_sum = 0.0f;
        float fallback_tap_gain_sum = 0.0f;
        const CazEnvSupervisorMetrics supervisor_metrics = state.supervisor_metrics;
        const uint64_t supervisor_events = supervisor_metric_total(&supervisor_metrics);
        uint64_t blocked_movement_sum = 0u;

        for (int index = 0; index < CAZ_ENV_DROID_COUNT; index++) {
            const CazEnvDroid *droid = &state.droids[index];
            if (droid->mode < 7u) {
                final_modes[droid->mode]++;
            }
            if (droid->charge < final_minimum_charge) {
                final_minimum_charge = droid->charge;
            }
            if (tracks[index].minimum_charge < observed_minimum_charge) {
                observed_minimum_charge = tracks[index].minimum_charge;
            }
            final_charge_sum += droid->charge;
            charger_gain_sum += droid->charger_gain;
            solar_gain_sum += droid->solar_gain;
            passive_solar_gain_sum += droid->passive_solar_gain;
            nav_solar_gain_sum += droid->nav_solar_gain;
            fallback_solar_gain_sum += droid->fallback_solar_gain;
            tap_gain_sum += droid->tap_gain;
            nav_tap_gain_sum += droid->nav_tap_gain;
            fallback_tap_gain_sum += droid->fallback_tap_gain;
            ever_charging += tracks[index].ever_charging ? 1 : 0;
            ever_depleted += tracks[index].ever_depleted ? 1 : 0;
            ever_returning += tracks[index].ever_returning ? 1 : 0;
            ever_solar += tracks[index].ever_solar ? 1 : 0;
            ever_tapping += tracks[index].ever_tapping ? 1 : 0;
            ever_zero_charge += tracks[index].ever_zero_charge ? 1 : 0;
            ever_nav_charger += tracks[index].ever_nav_charger ? 1 : 0;
            ever_nav_solar += tracks[index].ever_nav_solar ? 1 : 0;
            ever_nav_junction += tracks[index].ever_nav_junction ? 1 : 0;
            ever_bytecode_recovery += tracks[index].ever_bytecode_recovery ? 1 : 0;
            ever_supervisor_recovery += tracks[index].ever_supervisor_recovery ? 1 : 0;
            final_zero_charge += droid->charge <= 0.000001f ? 1 : 0;
            blocked_droids += droid->blocked_movement_count > 0u ? 1 : 0;
            blocked_movement_sum += droid->blocked_movement_count;
        }

        printf("seed=0x%08x case=%s final_min=%.3f observed_min=%.3f final_avg=%.3f",
               seed,
               matrix_case_name(options->matrix_case),
               final_minimum_charge,
               observed_minimum_charge,
               final_charge_sum / (float)CAZ_ENV_DROID_COUNT);
        printf(" ever: return=%d charge=%d solar=%d tap=%d zero=%d depleted=%d",
               ever_returning,
               ever_charging,
               ever_solar,
               ever_tapping,
               ever_zero_charge,
               ever_depleted);
        printf(" final_zero=%d gains: charger=%.3f solar=%.3f passive=%.3f requested=%.3f fallback_solar=%.3f tap=%.3f nav_tap=%.3f fallback_tap=%.3f final_modes:",
               final_zero_charge,
               charger_gain_sum,
               solar_gain_sum,
               passive_solar_gain_sum,
               nav_solar_gain_sum,
               fallback_solar_gain_sum,
               tap_gain_sum,
               nav_tap_gain_sum,
               fallback_tap_gain_sum);
        for (unsigned mode = 0; mode < 7u; mode++) {
            printf(" %s=%d", mode_name(mode), final_modes[mode]);
        }
        printf(" nav: charger=%d solar=%d junction=%d cause: bytecode-nav=%d supervisor=%d",
               ever_nav_charger,
               ever_nav_solar,
               ever_nav_junction,
               ever_bytecode_recovery,
               ever_supervisor_recovery);
        printf(" obstacles: droids=%d blocked=%llu",
               blocked_droids,
               (unsigned long long)blocked_movement_sum);
        printf(" fallback-events: charger=%llu solar=%llu junction=%llu loiter=%llu speed=%llu gait=%llu total=%llu",
               (unsigned long long)supervisor_metrics.charger_returns,
               (unsigned long long)supervisor_metrics.solar_forages,
               (unsigned long long)supervisor_metrics.junction_taps,
               (unsigned long long)supervisor_metrics.charger_loiters,
               (unsigned long long)supervisor_metrics.speed_overrides,
               (unsigned long long)supervisor_metrics.gait_overrides,
               (unsigned long long)supervisor_events);
        const float fallback_gain_sum = fallback_solar_gain_sum + fallback_tap_gain_sum;
        const int strict_failed = strict && (supervisor_events > 0u || fallback_gain_sum > 0.000001f);
        const int seed_failed = ever_zero_charge > 0 ||
                                ever_depleted > 0 ||
                                final_zero_charge > 0 ||
                                strict_failed;
        if (strict) {
            printf(" strict=%s", strict_failed ? "FAIL" : "ok");
        }
        if (seed_failed) {
            failed_seeds++;
        }
        printf(" result=%s\n", seed_failed ? "FAIL" : "PASS");
        if (seed_index == 0) {
            count_bytecode_runtimes(&state);
            print_sample_ports(&state);
            print_sample_output(&state, 0);
            print_sample_output(&state, 7);
        }
    }

    print_failure_report(&first_failure);
    if (failed_seeds > 0) {
        write_failure_artifact(&first_failure, options);
    }
    printf("survival-result=%s failed_seeds=%d/%d mode=%s fallback=%s\n",
           failed_seeds == 0 ? "PASS" : "FAIL",
           failed_seeds,
           options->seed_count,
           harness_mode_name(options->mode),
           fallback_policy_name(options->mode));
    return failed_seeds == 0 ? 0 : 1;
}

static int run_survival_matrix(const HarnessOptions *options)
{
    int failed_cases = 0;
    for (int matrix_case = 0; matrix_case < (int)MATRIX_CASE_COUNT; matrix_case++) {
        HarnessOptions case_options = *options;
        case_options.matrix_case = (MatrixCase)matrix_case;
        printf("matrix-case-start name=%s days=%d seeds=%d strict=1\n",
               matrix_case_name(case_options.matrix_case),
               case_options.days,
               case_options.seed_count);
        if (run_survival(&case_options) != 0) {
            failed_cases++;
        }
    }
    printf("matrix-result=%s failed_cases=%d/%d days=%d seeds=%d\n",
           failed_cases == 0 ? "PASS" : "FAIL",
           failed_cases,
           (int)MATRIX_CASE_COUNT,
           options->days,
           options->seed_count);
    return failed_cases == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    HarnessOptions options;
    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }

    printf("cazenv harness mode=%s fallback=%s days=%d seeds=%d dt=0.2s droids=%d\n",
           harness_mode_name(options.mode),
           fallback_policy_name(options.mode),
           options.days,
           options.seed_count,
           caz_env_droid_count());

    if (options.mode == HARNESS_MODE_PROBES) {
        return run_all_probes() ? 0 : 1;
    }
    if (options.mode == HARNESS_MODE_REGRESSION) {
        return run_regression_suite() ? 0 : 1;
    }
    if (options.mode == HARNESS_MODE_STRESS) {
        return run_stress_suite() ? 0 : 1;
    }
    if (options.mode == HARNESS_MODE_MATRIX) {
        return run_survival_matrix(&options);
    }
    return run_survival(&options);
}
