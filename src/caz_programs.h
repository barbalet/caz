#ifndef CAZ_PROGRAMS_H
#define CAZ_PROGRAMS_H

#include "caz_cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CAZ_PROGRAM_BASE CAZ_RESET_VECTOR
#define CAZ_PROGRAM_MAX 2048u

typedef enum CazProgramKind {
    CAZ_PROGRAM_CURIOUS_PATROL = 0,
    CAZ_PROGRAM_NAP_WATCH,
    CAZ_PROGRAM_FARMYARD_MOUSER
} CazProgramKind;

typedef struct CazProgramImage {
    uint8_t bytes[CAZ_PROGRAM_MAX];
    size_t length;
    const char *name;
    const char *description;
} CazProgramImage;

bool caz_program_build(CazProgramKind kind, CazProgramImage *image);
bool caz_program_load(CazCpu *cpu, CazProgramKind kind, CazProgramImage *image);
bool caz_program_parse(const char *name, CazProgramKind *kind);
const char *caz_program_name(CazProgramKind kind);
const char *caz_program_description(CazProgramKind kind);
void caz_program_print_all(FILE *out);

#endif
