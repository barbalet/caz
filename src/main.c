#include "caz_cpu.h"
#include "caz_droid.h"
#include "caz_programs.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Options {
    CazProgramKind program;
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
            "  --program NAME              curious-patrol, nap-watch, farmyard-mouser\n"
            "  --scenario NAME             kitchen, farmyard, night-parlour, hedgerow\n"
            "  --steps N                   body ticks to simulate (default: 64)\n"
            "  --instructions-per-tick N   Caz CPU instructions per body tick (default: 48)\n"
            "  --sample-every N            print one report every N body ticks (default: 1)\n"
            "  --seed N                    deterministic environment seed\n"
            "  --trace                     print CPU instruction trace to stderr\n"
            "  --quiet                     only print summary\n"
            "  --list                      list programs and scenarios\n"
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
    options->program = CAZ_PROGRAM_FARMYARD_MOUSER;
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
            caz_program_print_all(stdout);
            printf("\nScenarios:\n");
            caz_scenario_print_all(stdout);
            exit(0);
        } else if (strcmp(argv[i], "--trace") == 0) {
            options->trace = true;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            options->quiet = true;
        } else if (strcmp(argv[i], "--program") == 0 && i + 1 < argc) {
            i++;
            if (!caz_program_parse(argv[i], &options->program)) {
                fprintf(stderr, "Unknown program: %s\n", argv[i]);
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

    return true;
}

int main(int argc, char **argv)
{
    Options options;
    CazDroid droid;
    CazCpu cpu;
    CazProgramImage image;
    uint64_t step;
    int status = 0;

    if (!parse_args(argc, argv, &options)) {
        print_usage(stderr, argv[0]);
        return 2;
    }

    caz_droid_init(&droid, options.scenario, options.seed);
    caz_cpu_init(&cpu, caz_droid_read_port, caz_droid_write_port, &droid);
    cpu.trace = options.trace;

    if (!caz_program_load(&cpu, options.program, &image)) {
        fprintf(stderr, "Failed to build/load Caz program: %s\n", caz_program_name(options.program));
        return 1;
    }

    if (!options.quiet) {
        printf("Caz Cat Operating System simulation\n");
        printf("program=%s (%s)\n", image.name, image.description);
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
    }

    if (!options.quiet) {
        printf("\nCPU: ");
    }
    caz_cpu_dump(&cpu, stdout);
    return status;
}
