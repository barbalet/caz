#ifndef CAZ_LOADER_H
#define CAZ_LOADER_H

#include "caz_cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CAZ_PROGRAM_BASE CAZ_RESET_VECTOR
#define CAZ_PROGRAM_MAX 4096u
#define CAZ_PROGRAM_NAME_MAX 64u
#define CAZ_PROGRAM_DESCRIPTION_MAX 192u
#define CAZ_PROGRAM_PATH_MAX 512u
#define CAZ_LOADER_ERROR_MAX 256u

typedef enum CazProgramKind {
    CAZ_PROGRAM_CURIOUS_PATROL = 0,
    CAZ_PROGRAM_NAP_WATCH,
    CAZ_PROGRAM_FARMYARD_MOUSER,
    CAZ_PROGRAM_LOAF_AND_GROOM,
    CAZ_PROGRAM_STALK_AND_POUNCE,
    CAZ_PROGRAM_FARMYARD_CAUTION,
    CAZ_PROGRAM_GREETING_PLAY,
    CAZ_PROGRAM_TERRITORY_PATROL,
    CAZ_PROGRAM_FERAL_FORAGER,
    CAZ_PROGRAM_ENERGY_AWARE_HUNTER,
    CAZ_PROGRAM_COUNT
} CazProgramKind;

typedef struct CazProgramMetadata {
    CazProgramKind kind;
    const char *name;
    const char *description;
    uint8_t survival_participant;
    uint8_t cazenv_assignable;
    uint8_t standalone_energy;
    float default_speed;
    uint8_t default_gait;
} CazProgramMetadata;

typedef struct CazProgramImage {
    uint8_t bytes[CAZ_PROGRAM_MAX];
    size_t length;
    char name[CAZ_PROGRAM_NAME_MAX];
    char description[CAZ_PROGRAM_DESCRIPTION_MAX];
    char path[CAZ_PROGRAM_PATH_MAX];
    char error[CAZ_LOADER_ERROR_MAX];
} CazProgramImage;

bool caz_loader_assemble_source(const char *source, const char *source_name, CazProgramImage *image);
bool caz_loader_load_file(CazCpu *cpu, const char *path, CazProgramImage *image);
bool caz_loader_load_named(CazCpu *cpu,
                           CazProgramKind kind,
                           const char *program_dir,
                           CazProgramImage *image);
bool caz_loader_program_path(CazProgramKind kind,
                             const char *program_dir,
                             char *buffer,
                             size_t buffer_length);
bool caz_loader_parse_program_name(const char *name, CazProgramKind *kind);
size_t caz_loader_program_count(void);
const CazProgramMetadata *caz_loader_program_metadata(CazProgramKind kind);
const CazProgramMetadata *caz_loader_program_metadata_at(size_t index);
const char *caz_loader_program_name(CazProgramKind kind);
const char *caz_loader_program_description(CazProgramKind kind);
void caz_loader_print_programs(FILE *out);

#endif
