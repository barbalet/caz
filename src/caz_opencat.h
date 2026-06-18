#ifndef CAZ_OPENCAT_H
#define CAZ_OPENCAT_H

#include "caz_droid.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define CAZ_OPENCAT_ERROR_MAX 256u
#define CAZ_OPENCAT_PATH_MAX 256u

typedef enum CazOpenCatMode {
    CAZ_OPENCAT_DISABLED = 0,
    CAZ_OPENCAT_DRY_RUN,
    CAZ_OPENCAT_LIVE
} CazOpenCatMode;

typedef enum CazOpenCatModel {
    CAZ_OPENCAT_NYBBLE = 0,
    CAZ_OPENCAT_BITTLE,
    CAZ_OPENCAT_CAZ_DROID
} CazOpenCatModel;

typedef struct CazOpenCatCalibrationJoint {
    bool enabled;
    uint8_t servo;
    uint8_t minimum;
    uint8_t maximum;
} CazOpenCatCalibrationJoint;

typedef struct CazOpenCatCalibration {
    bool loaded_from_file;
    CazOpenCatModel model;
    uint64_t rate_limit_ticks;
    CazOpenCatCalibrationJoint joints[CAZ_BODY_JOINT_COUNT];
} CazOpenCatCalibration;

typedef struct CazOpenCatBridge {
    CazOpenCatMode mode;
    CazOpenCatModel model;
    CazOpenCatCalibration calibration;
    FILE *live_stream;
    uint64_t last_emit_tick;
    uint8_t last_skill;
    uint8_t last_reflex;
    uint8_t last_targets[CAZ_BODY_JOINT_COUNT];
    bool has_last;
} CazOpenCatBridge;

void caz_opencat_bridge_init(CazOpenCatBridge *bridge);
bool caz_opencat_model_parse(const char *name, CazOpenCatModel *model);
const char *caz_opencat_model_name(CazOpenCatModel model);
bool caz_opencat_calibration_load(const char *path,
                                  CazOpenCatCalibration *calibration,
                                  char *error,
                                  size_t error_length);
bool caz_opencat_bridge_open(CazOpenCatBridge *bridge,
                             CazOpenCatMode mode,
                             CazOpenCatModel model,
                             const char *serial_path,
                             const char *calibration_path,
                             uint64_t rate_limit_ticks,
                             char *error,
                             size_t error_length);
void caz_opencat_bridge_close(CazOpenCatBridge *bridge);
bool caz_opencat_bridge_emit(CazOpenCatBridge *bridge,
                             const CazDroid *droid,
                             FILE *dry_run_out);

#endif
