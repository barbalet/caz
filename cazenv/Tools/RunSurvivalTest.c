#include "../c_core/caz_env.h"
#include "../../src/caz_droid.h"

#include <dirent.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECOVERY_TRACE_COUNT 8u

typedef enum HarnessMode {
    HARNESS_MODE_PROBES,
    HARNESS_MODE_SHORT_COMPAT,
    HARNESS_MODE_SHORT_STRICT,
    HARNESS_MODE_LONG_COMPAT,
    HARNESS_MODE_LONG_STRICT
} HarnessMode;

typedef struct HarnessOptions {
    HarnessMode mode;
    int days;
    int seed_count;
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
    float passive_solar_gain;
    float nav_solar_gain;
    float tap_gain;
    float nav_tap_gain;
    float fallback_tap_gain;
    uint8_t mode;
    uint8_t energy_source;
    uint8_t nav_intent;
    uint8_t nav_status;
    uint8_t nav_cause;
    uint8_t charger_slots;
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
    case HARNESS_MODE_SHORT_COMPAT:
        return "short-compat";
    case HARNESS_MODE_SHORT_STRICT:
        return "short-strict";
    case HARNESS_MODE_LONG_COMPAT:
        return "long-compat";
    case HARNESS_MODE_LONG_STRICT:
        return "long-strict";
    default:
        return "unknown";
    }
}

static const char *fallback_policy_name(HarnessMode mode)
{
    switch (mode) {
    case HARNESS_MODE_PROBES:
        return "targeted-probes";
    case HARNESS_MODE_SHORT_COMPAT:
    case HARNESS_MODE_LONG_COMPAT:
        return "allowed-counted";
    case HARNESS_MODE_SHORT_STRICT:
    case HARNESS_MODE_LONG_STRICT:
        return "fail-on-use";
    default:
        return "unknown";
    }
}

static int is_strict_mode(HarnessMode mode)
{
    return mode == HARNESS_MODE_SHORT_STRICT || mode == HARNESS_MODE_LONG_STRICT;
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
    } else if (strcmp(name, "short-compat") == 0 || strcmp(name, "compat") == 0) {
        *out_mode = HARNESS_MODE_SHORT_COMPAT;
    } else if (strcmp(name, "short-strict") == 0 || strcmp(name, "strict") == 0) {
        *out_mode = HARNESS_MODE_SHORT_STRICT;
    } else if (strcmp(name, "long-compat") == 0) {
        *out_mode = HARNESS_MODE_LONG_COMPAT;
    } else if (strcmp(name, "long-strict") == 0) {
        *out_mode = HARNESS_MODE_LONG_STRICT;
    } else {
        return 0;
    }
    return 1;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--mode probes|short-compat|short-strict|long-compat|long-strict] [days] [seeds]\n",
            program);
}

static int parse_options(int argc, char **argv, HarnessOptions *options)
{
    int positional_count = 0;
    int days_was_set = 0;
    int seeds_was_set = 0;

    options->mode = HARNESS_MODE_SHORT_COMPAT;
    options->days = 14;
    options->seed_count = 3;

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
    if ((options->mode == HARNESS_MODE_LONG_COMPAT || options->mode == HARNESS_MODE_LONG_STRICT) &&
        !seeds_was_set) {
        options->seed_count = 5;
    }
    if (options->mode == HARNESS_MODE_PROBES) {
        options->days = 0;
        options->seed_count = 0;
    }
    if (options->mode != HARNESS_MODE_PROBES &&
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
    printf("sample-ports droid=0 eye=(luma=%u motion=%u edge=%u colour=%u) ear=(volume=%u pitch=%u bearing=%u pattern=%u) body=(roll=%u pitch=%u lifted=%u dropped=%u terrain=%u reflex=%u) survival=(battery=%u charger=%u/%u/%u junction=%u/%u solar=%u energy=%u nav=%u/%u)\n",
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
           caz_env_debug_read_port(state, 0, CAZ_PORT_ENERGY_SOURCE),
           caz_env_debug_read_port(state, 0, CAZ_PORT_NAV_INTENT),
           caz_env_debug_read_port(state, 0, CAZ_PORT_NAV_STATUS));
}

static void print_sample_output(const CazEnvState *state, int index)
{
    const CazEnvDroid *droid = &state->droids[index];
    const CazEnvBytecodeRuntime *runtime = &droid->bytecode;
    printf("sample-output droid=%d program=%s output=(nav=%u status=%u cause=%u gait=%u skill=%u head=%u ear=%u tail=%u vocal=%u eyelid=%u) motion=(mode=%u speed=%.2f target=%.2f/%.2f transitions=%u)\n",
           index,
           caz_env_bytecode_program_name(state, index),
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
    report->passive_solar_gain = droid->passive_solar_gain;
    report->nav_solar_gain = droid->nav_solar_gain;
    report->tap_gain = droid->tap_gain;
    report->nav_tap_gain = droid->nav_tap_gain;
    report->fallback_tap_gain = droid->fallback_tap_gain;
    report->mode = droid->mode;
    report->energy_source = droid->energy_source;
    report->nav_intent = droid->bytecode.output.nav_intent;
    report->nav_status = droid->nav_status;
    report->nav_cause = droid->nav_cause;
    report->charger_slots = caz_env_debug_read_port(state, droid_index, CAZ_PORT_CHARGER_SLOTS);

    report->recent_count = track->recovery_trace_count;
    const unsigned start = track->recovery_trace_count < RECOVERY_TRACE_COUNT
                         ? 0u
                         : track->recovery_trace_next;
    for (unsigned index = 0; index < report->recent_count; index++) {
        report->recent[index] = track->recovery_trace[(start + index) % RECOVERY_TRACE_COUNT];
    }
}

static void print_failure_report(const FailureReport *report)
{
    if (!report->captured) {
        return;
    }

    printf("first-failure seed=0x%08x step=%ld droid=%d program=%s charge=%.6f pos=%.2f/%.2f mode=%s energy=%u output=(nav=%u status=%u cause=%u) charger_slots=%u distances=(charger=%.2f junction=%.2f) gains=(passive=%.6f requested_solar=%.6f tap=%.6f nav_tap=%.6f fallback_tap=%.6f)\n",
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
           report->charger_distance,
           report->junction_distance,
           report->passive_solar_gain,
           report->nav_solar_gain,
           report->tap_gain,
           report->nav_tap_gain,
           report->fallback_tap_gain);
    printf("first-failure-recent");
    for (unsigned index = 0; index < report->recent_count; index++) {
        const RecoveryTrace *trace = &report->recent[index];
        printf(" [step=%ld charge=%.4f mode=%s energy=%u nav=%u status=%u cause=%u]",
               trace->step,
               trace->charge,
               mode_name(trace->mode),
               trace->energy_source,
               trace->nav_intent,
               trace->nav_status,
               trace->nav_cause);
    }
    printf("\n");
}

static int load_probe_programs(CazEnvState *state, char *load_error, size_t load_error_length)
{
    if (!caz_env_load_programs(state, "programs", load_error, load_error_length)) {
        fprintf(stderr, "%s\n", load_error);
        return 0;
    }
    return 1;
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

    state.droids[index].x = state.fixtures[0].x - 4.0f;
    state.droids[index].z = state.fixtures[0].z;
    state.droids[index].charge = 0.40f;
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

    for (int slot = 0; slot < CAZ_ENV_CHARGE_SLOT_COUNT; slot++) {
        state.charge_slots[slot] = (int8_t)slot;
    }
    state.droids[index].x = state.fixtures[0].x + 0.4f;
    state.droids[index].z = state.fixtures[0].z + 0.4f;
    state.droids[index].charge = 0.40f;
    state.droids[index].feral = 0.0f;
    state.droids[index].mode = CAZ_ENV_DROID_PROGRAM;
    state.droids[index].nav_cause = CAZ_ENV_NAV_CAUSE_NONE;

    for (int step = 0; step < 90; step++) {
        caz_env_step(&state, 0.2f);
    }

    printf("full-charger-probe ");
    print_sample_output(&state, index);
    return state.droids[index].bytecode.output.nav_intent == CAZ_NAV_CHARGER &&
           state.droids[index].mode != CAZ_ENV_DROID_CHARGING &&
           state.droids[index].nav_status == CAZ_NAV_STATUS_BLOCKED &&
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

    state.fixtures[17].x = 0.0f;
    state.fixtures[17].z = 0.0f;
    state.droids[index].x = 0.0f;
    state.droids[index].z = 0.0f;
    state.droids[index].charge = 0.30f;
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

static int run_all_probes(void)
{
    if (!run_program_registry_probe()) {
        fprintf(stderr, "program-registry-probe failed: programs/ archive and loader registry differ\n");
        return 0;
    }
    if (!run_nav_probe()) {
        fprintf(stderr, "nav-probe failed: return-to-charge.caz did not request a usable recovery navigation mode\n");
        return 0;
    }
    if (!run_charger_probe()) {
        fprintf(stderr, "charger-probe failed: bytecode NAV_CHARGER did not dock and charge\n");
        return 0;
    }
    if (!run_full_charger_probe()) {
        fprintf(stderr, "full-charger-probe failed: full charger did not block or loiter without docking\n");
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
    printf("probe-result=PASS\n");
    return 1;
}

static void init_track(DroidTrack *track)
{
    memset(track, 0, sizeof(*track));
    track->minimum_charge = FLT_MAX;
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
        if (seed_index == 0) {
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
        float final_minimum_charge = FLT_MAX;
        float observed_minimum_charge = FLT_MAX;
        float final_charge_sum = 0.0f;
        float solar_gain_sum = 0.0f;
        float passive_solar_gain_sum = 0.0f;
        float nav_solar_gain_sum = 0.0f;
        float tap_gain_sum = 0.0f;
        float nav_tap_gain_sum = 0.0f;
        float fallback_tap_gain_sum = 0.0f;
        const CazEnvSupervisorMetrics supervisor_metrics = state.supervisor_metrics;
        const uint64_t supervisor_events = supervisor_metric_total(&supervisor_metrics);

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
            solar_gain_sum += droid->solar_gain;
            passive_solar_gain_sum += droid->passive_solar_gain;
            nav_solar_gain_sum += droid->nav_solar_gain;
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
        }

        printf("seed=0x%08x final_min=%.3f observed_min=%.3f final_avg=%.3f",
               seed,
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
        printf(" final_zero=%d gains: solar=%.3f passive=%.3f requested=%.3f tap=%.3f nav_tap=%.3f fallback_tap=%.3f final_modes:",
               final_zero_charge,
               solar_gain_sum,
               passive_solar_gain_sum,
               nav_solar_gain_sum,
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
        printf(" fallback-events: charger=%llu solar=%llu junction=%llu loiter=%llu speed=%llu gait=%llu total=%llu",
               (unsigned long long)supervisor_metrics.charger_returns,
               (unsigned long long)supervisor_metrics.solar_forages,
               (unsigned long long)supervisor_metrics.junction_taps,
               (unsigned long long)supervisor_metrics.charger_loiters,
               (unsigned long long)supervisor_metrics.speed_overrides,
               (unsigned long long)supervisor_metrics.gait_overrides,
               (unsigned long long)supervisor_events);
        const int strict_failed = strict && supervisor_events > 0u;
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
    printf("survival-result=%s failed_seeds=%d/%d mode=%s fallback=%s\n",
           failed_seeds == 0 ? "PASS" : "FAIL",
           failed_seeds,
           options->seed_count,
           harness_mode_name(options->mode),
           fallback_policy_name(options->mode));
    return failed_seeds == 0 ? 0 : 1;
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
    return run_survival(&options);
}
