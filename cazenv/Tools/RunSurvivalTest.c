#include "../c_core/caz_env.h"
#include "../../src/caz_droid.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

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
} DroidTrack;

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

static int run_nav_probe(void)
{
    CazEnvState state;
    char load_error[512];
    const int index = 9;

    caz_env_init(&state, 0x0ca7e042u);
    if (!caz_env_load_programs(&state, "programs", load_error, sizeof(load_error))) {
        fprintf(stderr, "%s\n", load_error);
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

int main(int argc, char **argv)
{
    const int days = argc > 1 ? atoi(argv[1]) : 14;
    const int seed_count = argc > 2 ? atoi(argv[2]) : 3;
    const float dt_seconds = 0.2f;
    const long steps_per_day = (long)(24.0f * 60.0f * 60.0f / dt_seconds);
    const long total_steps = steps_per_day * (long)days;

    if (days <= 0 || seed_count <= 0) {
        fprintf(stderr, "usage: RunSurvivalTest [positive-days] [positive-seed-count]\n");
        return 2;
    }

    printf("cazenv survival test days=%d seeds=%d dt=%.1fs droids=%d\n",
           days,
           seed_count,
           dt_seconds,
           caz_env_droid_count());

    if (!run_nav_probe()) {
        fprintf(stderr, "nav-probe failed: return-to-charge.caz did not request a usable recovery navigation mode\n");
        return 1;
    }

    int failed_seeds = 0;
    for (int seed_index = 0; seed_index < seed_count; seed_index++) {
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
            tracks[index].minimum_charge = FLT_MAX;
            tracks[index].ever_charging = 0u;
            tracks[index].ever_depleted = 0u;
            tracks[index].ever_returning = 0u;
            tracks[index].ever_solar = 0u;
            tracks[index].ever_tapping = 0u;
            tracks[index].ever_zero_charge = 0u;
            tracks[index].ever_nav_charger = 0u;
            tracks[index].ever_nav_solar = 0u;
            tracks[index].ever_nav_junction = 0u;
            tracks[index].ever_bytecode_recovery = 0u;
            tracks[index].ever_supervisor_recovery = 0u;
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
        float tap_gain_sum = 0.0f;

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
            tap_gain_sum += droid->tap_gain;
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
        printf(" final_zero=%d gains: solar=%.3f tap=%.3f final_modes:",
               final_zero_charge,
               solar_gain_sum,
               tap_gain_sum);
        for (unsigned mode = 0; mode < 7u; mode++) {
            printf(" %s=%d", mode_name(mode), final_modes[mode]);
        }
        printf(" nav: charger=%d solar=%d junction=%d cause: bytecode-nav=%d supervisor=%d",
               ever_nav_charger,
               ever_nav_solar,
               ever_nav_junction,
               ever_bytecode_recovery,
               ever_supervisor_recovery);
        const int seed_failed = ever_zero_charge > 0 || ever_depleted > 0 || final_zero_charge > 0;
        if (seed_failed) {
            failed_seeds++;
        }
        printf(" result=%s\n", seed_failed ? "FAIL" : "PASS");
        if (seed_index == 0) {
            count_bytecode_runtimes(&state);
            print_sample_ports(&state);
            print_sample_output(&state, 0);
            print_sample_output(&state, 9);
        }
    }

    printf("survival-result=%s failed_seeds=%d/%d\n",
           failed_seeds == 0 ? "PASS" : "FAIL",
           failed_seeds,
           seed_count);
    return failed_seeds == 0 ? 0 : 1;
}
