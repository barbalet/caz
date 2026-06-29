#include "caz_cpu.h"
#include "caz_core.h"
#include "caz_droid.h"
#include "caz_loader.h"
#include "caz_opencat.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    CazProgramKind program;
    char program_path[CAZ_PROGRAM_PATH_MAX];
    char program_dir[CAZ_PROGRAM_PATH_MAX];
    char skills_dir[CAZ_PROGRAM_PATH_MAX];
    char opencat_serial_path[CAZ_OPENCAT_PATH_MAX];
    char opencat_calibration_path[CAZ_OPENCAT_PATH_MAX];
    bool custom_program_path;
    CazOpenCatMode opencat_mode;
    CazOpenCatModel opencat_model;
    uint64_t opencat_rate_limit_ticks;
    CazScenario scenario;
    uint32_t seed;
    uint64_t steps;
    uint64_t instructions_per_tick;
    uint64_t sample_every;
    bool trace;
    bool quiet;
} Options;

static void print_usage(FILE *out, const char *argv0)
{
    fprintf(out,
            "Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  --program NAME|PATH         named archive program or a .caz file\n"
            "  --program-dir PATH          directory for named .caz programs (default: programs)\n"
            "  --skills-dir PATH           directory for .cazskill skill overrides (default: skills)\n"
            "  --opencat-dry-run           print OpenCat-style serial commands without writing hardware\n"
            "  --opencat-live              enable live OpenCat serial output; requires serial and calibration\n"
            "  --opencat-model NAME        nybble, bittle, or caz-droid (default: nybble)\n"
            "  --opencat-serial PATH       serial device/path for live OpenCat output\n"
            "  --opencat-calibration PATH  calibration limits required for live OpenCat output\n"
            "  --opencat-rate N            bridge command rate limit in body ticks\n"
            "  --scenario NAME             kitchen, farmyard, night-parlour, hedgerow\n"
            "  --steps N                   body ticks to simulate (default: 64)\n"
            "  --instructions-per-tick N   Caz CPU instructions per body tick (default: 48)\n"
            "  --sample-every N            print one report every N body ticks (default: 1)\n"
            "  --seed N                    deterministic environment seed\n"
            "  --trace                     print CPU instruction trace to stderr\n"
            "  --quiet                     only print summary\n"
            "  --list                      list programs and scenarios\n"
            "  --core-version              print CAZ_CORE_VERSION and exit\n"
            "  --version                   print Caz Core version and exit\n"
            "  --help                      show this help\n",
            argv0);
}

static bool parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 0);
    if (end == text || *end != '\0') {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_u32(const char *text, uint32_t *value)
{
    uint64_t parsed = 0u;
    if (!parse_u64(text, &parsed) || parsed > 0xffffffffull) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_args(int argc, char **argv, Options *options)
{
    int i;
    options->program = CAZ_PROGRAM_CURIOUS_PATROL;
    options->program_path[0] = '\0';
    snprintf(options->program_dir, sizeof(options->program_dir), "%s", "programs");
    snprintf(options->skills_dir, sizeof(options->skills_dir), "%s", "skills");
    options->opencat_serial_path[0] = '\0';
    options->opencat_calibration_path[0] = '\0';
    options->custom_program_path = false;
    options->opencat_mode = CAZ_OPENCAT_DISABLED;
    options->opencat_model = CAZ_OPENCAT_NYBBLE;
    options->opencat_rate_limit_ticks = 0u;
    options->scenario = CAZ_SCENARIO_FARMYARD;
    options->seed = 0u;
    options->steps = 64u;
    options->instructions_per_tick = 48u;
    options->sample_every = 1u;
    options->trace = false;
    options->quiet = false;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(stdout, argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--list") == 0) {
            printf("Programs:\n");
            caz_loader_print_programs(stdout);
            printf("\nScenarios:\n");
            caz_scenario_print_all(stdout);
            exit(0);
        } else if (strcmp(argv[i], "--core-version") == 0) {
            printf("%s\n", caz_core_version());
            exit(0);
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("Caz Core %s\n", caz_core_version());
            exit(0);
        } else if (strcmp(argv[i], "--trace") == 0) {
            options->trace = true;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            options->quiet = true;
        } else if (strcmp(argv[i], "--program") == 0 && i + 1 < argc) {
            i++;
            if (caz_loader_parse_program_name(argv[i], &options->program)) {
                options->custom_program_path = false;
                options->program_path[0] = '\0';
            } else {
                snprintf(options->program_path, sizeof(options->program_path), "%s", argv[i]);
                options->custom_program_path = true;
            }
        } else if (strcmp(argv[i], "--program-dir") == 0 && i + 1 < argc) {
            i++;
            if (strlen(argv[i]) >= sizeof(options->program_dir)) {
                fprintf(stderr, "Program directory path is too long: %s\n", argv[i]);
                return false;
            }
            snprintf(options->program_dir, sizeof(options->program_dir), "%s", argv[i]);
        } else if (strcmp(argv[i], "--skills-dir") == 0 && i + 1 < argc) {
            i++;
            if (strlen(argv[i]) >= sizeof(options->skills_dir)) {
                fprintf(stderr, "Skills directory path is too long: %s\n", argv[i]);
                return false;
            }
            snprintf(options->skills_dir, sizeof(options->skills_dir), "%s", argv[i]);
        } else if (strcmp(argv[i], "--opencat-dry-run") == 0) {
            if (options->opencat_mode == CAZ_OPENCAT_LIVE) {
                fprintf(stderr, "--opencat-dry-run and --opencat-live are mutually exclusive\n");
                return false;
            }
            options->opencat_mode = CAZ_OPENCAT_DRY_RUN;
        } else if (strcmp(argv[i], "--opencat-live") == 0) {
            if (options->opencat_mode == CAZ_OPENCAT_DRY_RUN) {
                fprintf(stderr, "--opencat-dry-run and --opencat-live are mutually exclusive\n");
                return false;
            }
            options->opencat_mode = CAZ_OPENCAT_LIVE;
        } else if (strcmp(argv[i], "--opencat-model") == 0 && i + 1 < argc) {
            i++;
            if (!caz_opencat_model_parse(argv[i], &options->opencat_model)) {
                fprintf(stderr, "Unknown OpenCat model: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--opencat-serial") == 0 && i + 1 < argc) {
            i++;
            if (strlen(argv[i]) >= sizeof(options->opencat_serial_path)) {
                fprintf(stderr, "OpenCat serial path is too long: %s\n", argv[i]);
                return false;
            }
            snprintf(options->opencat_serial_path, sizeof(options->opencat_serial_path), "%s", argv[i]);
        } else if (strcmp(argv[i], "--opencat-calibration") == 0 && i + 1 < argc) {
            i++;
            if (strlen(argv[i]) >= sizeof(options->opencat_calibration_path)) {
                fprintf(stderr, "OpenCat calibration path is too long: %s\n", argv[i]);
                return false;
            }
            snprintf(options->opencat_calibration_path, sizeof(options->opencat_calibration_path), "%s", argv[i]);
        } else if (strcmp(argv[i], "--opencat-rate") == 0 && i + 1 < argc) {
            i++;
            if (!parse_u64(argv[i], &options->opencat_rate_limit_ticks) ||
                options->opencat_rate_limit_ticks == 0u) {
                fprintf(stderr, "Invalid --opencat-rate value: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            i++;
            if (!caz_scenario_parse(argv[i], &options->scenario)) {
                fprintf(stderr, "Unknown scenario: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            i++;
            if (!parse_u64(argv[i], &options->steps)) {
                fprintf(stderr, "Invalid --steps value: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--instructions-per-tick") == 0 && i + 1 < argc) {
            i++;
            if (!parse_u64(argv[i], &options->instructions_per_tick) || options->instructions_per_tick == 0u) {
                fprintf(stderr, "Invalid --instructions-per-tick value: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--sample-every") == 0 && i + 1 < argc) {
            i++;
            if (!parse_u64(argv[i], &options->sample_every) || options->sample_every == 0u) {
                fprintf(stderr, "Invalid --sample-every value: %s\n", argv[i]);
                return false;
            }
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            i++;
            if (!parse_u32(argv[i], &options->seed)) {
                fprintf(stderr, "Invalid --seed value: %s\n", argv[i]);
                return false;
            }
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return false;
        }
    }

    if (options->opencat_serial_path[0] != '\0' &&
        options->opencat_mode != CAZ_OPENCAT_LIVE) {
        fprintf(stderr, "--opencat-serial is only accepted with --opencat-live\n");
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    Options options;
    CazDroid droid;
    CazCpu cpu;
    CazProgramImage image;
    CazOpenCatBridge opencat_bridge;
    char skill_error[CAZ_LOADER_ERROR_MAX];
    char opencat_error[CAZ_OPENCAT_ERROR_MAX];
    uint64_t step;
    int status = 0;

    if (!parse_args(argc, argv, &options)) {
        print_usage(stderr, argv[0]);
        return 2;
    }

    if (!caz_body_load_skill_dir(options.skills_dir, skill_error, sizeof(skill_error))) {
        fprintf(stderr, "Failed to load Caz skills from %s\n%s\n", options.skills_dir, skill_error);
        return 1;
    }

    caz_droid_init(&droid, options.scenario, options.seed);
    caz_cpu_init(&cpu, caz_droid_read_port, caz_droid_write_port, &droid);
    cpu.trace = options.trace;

    if (options.custom_program_path) {
        if (!caz_loader_load_file(&cpu, options.program_path, &image)) {
            fprintf(stderr, "Failed to load Caz program: %s\n%s\n", options.program_path, image.error);
            return 1;
        }
    } else if (!caz_loader_load_named(&cpu, options.program, options.program_dir, &image)) {
        fprintf(stderr,
                "Failed to load Caz program: %s\n%s\n",
                caz_loader_program_name(options.program),
                image.error);
        return 1;
    }

    if (!caz_opencat_bridge_open(&opencat_bridge,
                                 options.opencat_mode,
                                 options.opencat_model,
                                 options.opencat_serial_path,
                                 options.opencat_calibration_path,
                                 options.opencat_rate_limit_ticks,
                                 opencat_error,
                                 sizeof(opencat_error))) {
        fprintf(stderr, "Failed to configure OpenCat bridge\n%s\n", opencat_error);
        return 1;
    }

    if (!options.quiet) {
        printf("Caz Cat Operating System simulation\n");
        printf("core_version=%s\n", caz_core_version());
        printf("program=%s (%s)\n", image.name, image.description);
        printf("source=%s\n", image.path);
        printf("scenario=%s steps=%llu instructions_per_tick=%llu program_bytes=%zu\n\n",
               caz_scenario_name(options.scenario),
               (unsigned long long)options.steps,
               (unsigned long long)options.instructions_per_tick,
               image.length);
    }

    for (step = 0u; step < options.steps && !cpu.halted; step++) {
        uint64_t i;
        caz_droid_tick(&droid);
        for (i = 0u; i < options.instructions_per_tick && !cpu.halted; i++) {
            if (caz_cpu_step(&cpu) < 0) {
                status = 1;
                break;
            }
        }
        if (status != 0) {
            break;
        }
        if (!options.quiet && (step % options.sample_every) == 0u) {
            caz_droid_print_report(&droid, stdout);
        }
        caz_opencat_bridge_emit(&opencat_bridge, &droid, stdout);
    }

    caz_opencat_bridge_close(&opencat_bridge);

    if (!options.quiet) {
        printf("\nCPU: ");
    }
    caz_cpu_dump(&cpu, stdout);
    return status;
}
