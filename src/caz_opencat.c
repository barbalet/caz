#include "caz_opencat.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct CazOpenCatDefaultJoint {
    uint8_t joint;
    uint8_t servo;
    uint8_t minimum;
    uint8_t maximum;
} CazOpenCatDefaultJoint;

static const CazOpenCatDefaultJoint nybble_dry_run_joints[] = {
    {CAZ_BODY_JOINT_HEAD_YAW, 0u, 64u, 192u},
    {CAZ_BODY_JOINT_HEAD_PITCH, 1u, 72u, 184u},
    {CAZ_BODY_JOINT_LEFT_SHOULDER, 8u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_SHOULDER, 9u, 48u, 208u},
    {CAZ_BODY_JOINT_LEFT_HIP, 12u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_HIP, 13u, 48u, 208u},
    {CAZ_BODY_JOINT_LEFT_KNEE, 14u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_KNEE, 15u, 48u, 208u},
    {CAZ_BODY_JOINT_LEFT_ELBOW, 10u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_ELBOW, 11u, 48u, 208u}
};

static const CazOpenCatDefaultJoint bittle_dry_run_joints[] = {
    {CAZ_BODY_JOINT_LEFT_SHOULDER, 8u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_SHOULDER, 9u, 48u, 208u},
    {CAZ_BODY_JOINT_LEFT_HIP, 12u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_HIP, 13u, 48u, 208u},
    {CAZ_BODY_JOINT_LEFT_KNEE, 14u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_KNEE, 15u, 48u, 208u},
    {CAZ_BODY_JOINT_LEFT_ELBOW, 10u, 48u, 208u},
    {CAZ_BODY_JOINT_RIGHT_ELBOW, 11u, 48u, 208u}
};

static void set_error(char *error, size_t error_length, const char *message)
{
    if (error_length == 0u) {
        return;
    }
    snprintf(error, error_length, "%s", message);
}

static void set_path_error(char *error, size_t error_length, const char *path, const char *message)
{
    if (error_length == 0u) {
        return;
    }
    snprintf(error, error_length, "%s: %s", path, message);
}

static char *trim_text(char *text)
{
    char *end;
    while (isspace((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0') {
        return text;
    }
    end = text + strlen(text) - 1u;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return text;
}

static void normalize_key(char *dst, size_t dst_length, const char *src)
{
    size_t i = 0u;
    if (dst_length == 0u) {
        return;
    }
    while (src != NULL && *src != '\0' && i + 1u < dst_length) {
        unsigned char ch = (unsigned char)*src;
        if (!isspace(ch)) {
            dst[i++] = ch == '_' ? '-' : (char)tolower(ch);
        }
        src++;
    }
    dst[i] = '\0';
}

static bool parse_u8_text(const char *text, uint8_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 0);
    if (end == text || *trim_text(end) != '\0' || parsed > 255ul) {
        return false;
    }
    *value = (uint8_t)parsed;
    return true;
}

static bool parse_u64_text(const char *text, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed = strtoull(text, &end, 0);
    if (end == text || *trim_text(end) != '\0') {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static void calibration_clear(CazOpenCatCalibration *calibration, CazOpenCatModel model)
{
    size_t i;
    memset(calibration, 0, sizeof(*calibration));
    calibration->model = model;
    calibration->rate_limit_ticks = 4u;
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        calibration->joints[i].servo = (uint8_t)i;
        calibration->joints[i].minimum = 0u;
        calibration->joints[i].maximum = 255u;
    }
}

static void enable_default_joint(CazOpenCatCalibration *calibration,
                                 const CazOpenCatDefaultJoint *joint)
{
    calibration->joints[joint->joint].enabled = true;
    calibration->joints[joint->joint].servo = joint->servo;
    calibration->joints[joint->joint].minimum = joint->minimum;
    calibration->joints[joint->joint].maximum = joint->maximum;
}

static void load_default_calibration(CazOpenCatCalibration *calibration, CazOpenCatModel model)
{
    size_t i;
    calibration_clear(calibration, model);
    switch (model) {
    case CAZ_OPENCAT_NYBBLE:
        for (i = 0u; i < sizeof(nybble_dry_run_joints) / sizeof(nybble_dry_run_joints[0]); i++) {
            enable_default_joint(calibration, &nybble_dry_run_joints[i]);
        }
        break;
    case CAZ_OPENCAT_BITTLE:
        for (i = 0u; i < sizeof(bittle_dry_run_joints) / sizeof(bittle_dry_run_joints[0]); i++) {
            enable_default_joint(calibration, &bittle_dry_run_joints[i]);
        }
        break;
    case CAZ_OPENCAT_CAZ_DROID:
    default:
        for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
            calibration->joints[i].enabled = true;
            calibration->joints[i].servo = (uint8_t)i;
            calibration->joints[i].minimum = 32u;
            calibration->joints[i].maximum = 224u;
        }
        calibration->joints[CAZ_BODY_JOINT_TAIL_BASE].minimum = 0u;
        calibration->joints[CAZ_BODY_JOINT_TAIL_BASE].maximum = 255u;
        calibration->joints[CAZ_BODY_JOINT_TAIL_TIP].minimum = 0u;
        calibration->joints[CAZ_BODY_JOINT_TAIL_TIP].maximum = 255u;
        break;
    }
}

bool caz_opencat_model_parse(const char *name, CazOpenCatModel *model)
{
    char key[32];
    normalize_key(key, sizeof(key), name);
    if (strcmp(key, "nybble") == 0) {
        *model = CAZ_OPENCAT_NYBBLE;
        return true;
    }
    if (strcmp(key, "bittle") == 0) {
        *model = CAZ_OPENCAT_BITTLE;
        return true;
    }
    if (strcmp(key, "caz-droid") == 0 || strcmp(key, "caz") == 0) {
        *model = CAZ_OPENCAT_CAZ_DROID;
        return true;
    }
    return false;
}

const char *caz_opencat_model_name(CazOpenCatModel model)
{
    switch (model) {
    case CAZ_OPENCAT_NYBBLE: return "nybble";
    case CAZ_OPENCAT_BITTLE: return "bittle";
    case CAZ_OPENCAT_CAZ_DROID: return "caz-droid";
    default: return "unknown";
    }
}

static bool parse_joint_key(const char *key, uint8_t *joint)
{
    if (strncmp(key, "joint.", 6u) == 0) {
        return parse_u8_text(key + 6, joint) && *joint < CAZ_BODY_JOINT_COUNT;
    }
    return false;
}

static bool parse_joint_calibration(char *text, CazOpenCatCalibrationJoint *joint)
{
    char *first = strchr(text, ',');
    char *second;
    uint8_t servo;
    uint8_t minimum;
    uint8_t maximum;
    if (first == NULL) {
        return false;
    }
    *first = '\0';
    second = strchr(first + 1, ',');
    if (second == NULL) {
        return false;
    }
    *second = '\0';
    if (!parse_u8_text(trim_text(text), &servo) ||
        !parse_u8_text(trim_text(first + 1), &minimum) ||
        !parse_u8_text(trim_text(second + 1), &maximum) ||
        minimum > maximum) {
        return false;
    }
    joint->enabled = true;
    joint->servo = servo;
    joint->minimum = minimum;
    joint->maximum = maximum;
    return true;
}

bool caz_opencat_calibration_load(const char *path,
                                  CazOpenCatCalibration *calibration,
                                  char *error,
                                  size_t error_length)
{
    FILE *file = fopen(path, "rb");
    char line[256];
    int line_number = 0;
    CazOpenCatModel model = calibration->model;

    if (file == NULL) {
        set_path_error(error, error_length, path, "could not open calibration file");
        return false;
    }

    calibration_clear(calibration, model);
    calibration->loaded_from_file = true;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *text;
        char *equals;
        char key[64];
        uint8_t joint_index;
        line_number++;
        text = trim_text(line);
        if (*text == '\0' || *text == '#' || *text == ';') {
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL) {
            fclose(file);
            snprintf(error, error_length, "%s:%d: expected key=value", path, line_number);
            return false;
        }
        *equals = '\0';
        normalize_key(key, sizeof(key), trim_text(text));
        text = trim_text(equals + 1);

        if (strcmp(key, "model") == 0) {
            if (!caz_opencat_model_parse(text, &calibration->model)) {
                fclose(file);
                snprintf(error, error_length, "%s:%d: unknown model", path, line_number);
                return false;
            }
        } else if (strcmp(key, "rate-limit-ticks") == 0) {
            if (!parse_u64_text(text, &calibration->rate_limit_ticks) ||
                calibration->rate_limit_ticks == 0u) {
                fclose(file);
                snprintf(error, error_length, "%s:%d: invalid rate limit", path, line_number);
                return false;
            }
        } else if (parse_joint_key(key, &joint_index)) {
            if (!parse_joint_calibration(text, &calibration->joints[joint_index])) {
                fclose(file);
                snprintf(error, error_length, "%s:%d: expected servo,min,max for joint", path, line_number);
                return false;
            }
        } else {
            fclose(file);
            snprintf(error, error_length, "%s:%d: unknown calibration key", path, line_number);
            return false;
        }
    }

    fclose(file);
    return true;
}

void caz_opencat_bridge_init(CazOpenCatBridge *bridge)
{
    memset(bridge, 0, sizeof(*bridge));
    bridge->mode = CAZ_OPENCAT_DISABLED;
    bridge->model = CAZ_OPENCAT_NYBBLE;
    load_default_calibration(&bridge->calibration, bridge->model);
}

bool caz_opencat_bridge_open(CazOpenCatBridge *bridge,
                             CazOpenCatMode mode,
                             CazOpenCatModel model,
                             const char *serial_path,
                             const char *calibration_path,
                             uint64_t rate_limit_ticks,
                             char *error,
                             size_t error_length)
{
    caz_opencat_bridge_init(bridge);
    bridge->mode = mode;
    bridge->model = model;
    load_default_calibration(&bridge->calibration, model);

    if (calibration_path != NULL && calibration_path[0] != '\0') {
        bridge->calibration.model = model;
        if (!caz_opencat_calibration_load(calibration_path, &bridge->calibration, error, error_length)) {
            return false;
        }
        bridge->model = bridge->calibration.model;
    }

    if (rate_limit_ticks != 0u) {
        bridge->calibration.rate_limit_ticks = rate_limit_ticks;
    }

    if (mode == CAZ_OPENCAT_DISABLED) {
        return true;
    }

    if (mode == CAZ_OPENCAT_LIVE) {
        if (serial_path == NULL || serial_path[0] == '\0') {
            set_error(error, error_length, "live OpenCat output requires --opencat-serial PATH");
            return false;
        }
        if (calibration_path == NULL || calibration_path[0] == '\0') {
            set_error(error, error_length, "live OpenCat output requires --opencat-calibration PATH");
            return false;
        }
        bridge->live_stream = fopen(serial_path, "ab");
        if (bridge->live_stream == NULL) {
            snprintf(error,
                     error_length,
                     "%s: could not open serial output: %s",
                     serial_path,
                     strerror(errno));
            return false;
        }
    }

    return true;
}

void caz_opencat_bridge_close(CazOpenCatBridge *bridge)
{
    if (bridge->live_stream != NULL) {
        fclose(bridge->live_stream);
        bridge->live_stream = NULL;
    }
    bridge->mode = CAZ_OPENCAT_DISABLED;
}

static const char *skill_command(uint8_t skill)
{
    switch (skill) {
    case CAZ_BODY_SKILL_BALANCE: return "kbalance";
    case CAZ_BODY_SKILL_REST: return "krest";
    case CAZ_BODY_SKILL_SIT: return "ksit";
    case CAZ_BODY_SKILL_WALK: return "kwk";
    case CAZ_BODY_SKILL_CRAWL: return "kcr";
    case CAZ_BODY_SKILL_POUNCE: return "kpu";
    case CAZ_BODY_SKILL_SNIFF: return "ksnf";
    case CAZ_BODY_SKILL_SCRATCH: return "kscr";
    case CAZ_BODY_SKILL_GROOM: return "kgrm";
    case CAZ_BODY_SKILL_STRETCH: return "kstr";
    case CAZ_BODY_SKILL_STARTLE: return "kbk";
    case CAZ_BODY_SKILL_RECOVER: return "kbalance";
    default: return "";
    }
}

static bool targets_changed(const CazOpenCatBridge *bridge, const CazBody *body)
{
    size_t i;
    if (!bridge->has_last) {
        return true;
    }
    if (bridge->last_skill != body->active_skill ||
        bridge->last_reflex != body->reflex_state) {
        return true;
    }
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        if (bridge->last_targets[i] != body->joint_targets[i]) {
            return true;
        }
    }
    return false;
}

static void remember_targets(CazOpenCatBridge *bridge, const CazBody *body)
{
    size_t i;
    bridge->has_last = true;
    bridge->last_skill = body->active_skill;
    bridge->last_reflex = body->reflex_state;
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        bridge->last_targets[i] = body->joint_targets[i];
    }
}

static void write_live_command(CazOpenCatBridge *bridge, const char *command)
{
    if (bridge->live_stream != NULL) {
        fprintf(bridge->live_stream, "%s\n", command);
        fflush(bridge->live_stream);
    }
}

bool caz_opencat_bridge_emit(CazOpenCatBridge *bridge,
                             const CazDroid *droid,
                             FILE *dry_run_out)
{
    const CazBody *body = &droid->body;
    const char *skill = skill_command(body->active_skill);
    char joint_command[512];
    size_t used = 0u;
    size_t i;
    bool emitted_joint = false;
    bool blocked_joint = false;

    if (bridge->mode == CAZ_OPENCAT_DISABLED) {
        return true;
    }

    if (bridge->has_last &&
        droid->body_ticks > bridge->last_emit_tick &&
        droid->body_ticks - bridge->last_emit_tick < bridge->calibration.rate_limit_ticks) {
        return true;
    }
    if (!targets_changed(bridge, body)) {
        return true;
    }

    if (skill[0] != '\0') {
        if (bridge->mode == CAZ_OPENCAT_DRY_RUN) {
            fprintf(dry_run_out,
                    "opencat tick=%04llu model=%s mode=dry-run status=%s reflex=%s skill_cmd=%s\n",
                    (unsigned long long)droid->body_ticks,
                    caz_opencat_model_name(bridge->model),
                    caz_body_skill_status_name(body->skill_status),
                    caz_body_reflex_state_name(body->reflex_state),
                    skill);
        } else {
            write_live_command(bridge, skill);
        }
    }

    used = (size_t)snprintf(joint_command, sizeof(joint_command), "m");
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        const CazOpenCatCalibrationJoint *joint = &bridge->calibration.joints[i];
        uint8_t value = body->joint_targets[i];
        if (!joint->enabled) {
            continue;
        }
        if (value < joint->minimum || value > joint->maximum) {
            blocked_joint = true;
            continue;
        }
        if (used < sizeof(joint_command)) {
            used += (size_t)snprintf(joint_command + used,
                                     sizeof(joint_command) - used,
                                     " %u %u",
                                     joint->servo,
                                     value);
        }
        emitted_joint = true;
    }

    if (emitted_joint) {
        if (bridge->mode == CAZ_OPENCAT_DRY_RUN) {
            fprintf(dry_run_out,
                    "opencat tick=%04llu model=%s mode=dry-run joint_cmd=%s%s\n",
                    (unsigned long long)droid->body_ticks,
                    caz_opencat_model_name(bridge->model),
                    joint_command,
                    blocked_joint ? " block=out-of-calibration" : "");
        } else {
            write_live_command(bridge, joint_command);
        }
    } else if (bridge->mode == CAZ_OPENCAT_DRY_RUN && blocked_joint) {
        fprintf(dry_run_out,
                "opencat tick=%04llu model=%s mode=dry-run block=all-joints-out-of-calibration\n",
                (unsigned long long)droid->body_ticks,
                caz_opencat_model_name(bridge->model));
    }

    remember_targets(bridge, body);
    bridge->last_emit_tick = droid->body_ticks;
    return true;
}
