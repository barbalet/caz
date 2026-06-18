#include "caz_body.h"

#include "caz_droid.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CazBodyJointLimit {
    const char *name;
    uint8_t minimum;
    uint8_t maximum;
} CazBodyJointLimit;

typedef struct CazBodySkillDef {
    uint8_t skill;
    const char *name;
    uint8_t gait;
    uint8_t head_yaw;
    uint8_t ear_pose;
    uint8_t tail_pose;
    uint8_t vocal;
    uint8_t eyelid;
    uint8_t joints[CAZ_BODY_JOINT_COUNT];
} CazBodySkillDef;

static const CazBodyJointLimit joint_limits[CAZ_BODY_JOINT_COUNT] = {
    {"head-yaw", 32u, 224u},
    {"head-pitch", 64u, 192u},
    {"left-shoulder", 32u, 224u},
    {"right-shoulder", 32u, 224u},
    {"left-hip", 32u, 224u},
    {"right-hip", 32u, 224u},
    {"tail-base", 0u, 255u},
    {"tail-tip", 0u, 255u},
    {"spine-height", 56u, 200u},
    {"spine-curve", 64u, 192u},
    {"left-knee", 32u, 224u},
    {"right-knee", 32u, 224u},
    {"left-elbow", 32u, 224u},
    {"right-elbow", 32u, 224u},
    {"paw-spread", 64u, 200u},
    {"body-roll", 64u, 192u}
};

static const CazBodySkillDef built_in_skill_table[] = {
    {CAZ_BODY_SKILL_BALANCE, "balance", 2u, 128u, 2u, 5u, 0u, 220u,
     {128u, 128u, 118u, 138u, 118u, 138u, 128u, 128u, 116u, 128u, 116u, 140u, 118u, 138u, 132u, 128u}},
    {CAZ_BODY_SKILL_REST, "rest", 0u, 128u, 1u, 2u, 3u, 72u,
     {128u, 92u, 104u, 104u, 96u, 96u, 72u, 88u, 64u, 112u, 84u, 84u, 92u, 92u, 92u, 128u}},
    {CAZ_BODY_SKILL_SIT, "sit", 0u, 128u, 2u, 1u, 0u, 170u,
     {128u, 132u, 128u, 128u, 96u, 96u, 136u, 120u, 104u, 144u, 80u, 80u, 136u, 136u, 112u, 128u}},
    {CAZ_BODY_SKILL_WALK, "walk", 1u, 128u, 1u, 5u, 0u, 170u,
     {128u, 128u, 104u, 152u, 152u, 104u, 144u, 160u, 136u, 132u, 116u, 148u, 148u, 116u, 136u, 128u}},
    {CAZ_BODY_SKILL_CRAWL, "crawl", 2u, 128u, 2u, 3u, 0u, 212u,
     {128u, 116u, 92u, 164u, 164u, 92u, 104u, 116u, 72u, 112u, 96u, 156u, 156u, 96u, 172u, 128u}},
    {CAZ_BODY_SKILL_POUNCE, "pounce", 3u, 156u, 2u, 7u, 2u, 248u,
     {156u, 148u, 196u, 196u, 72u, 72u, 176u, 208u, 136u, 160u, 184u, 184u, 192u, 192u, 188u, 132u}},
    {CAZ_BODY_SKILL_SNIFF, "sniff", 5u, 116u, 2u, 4u, 1u, 214u,
     {116u, 160u, 120u, 136u, 132u, 124u, 144u, 152u, 118u, 144u, 128u, 128u, 124u, 136u, 108u, 124u}},
    {CAZ_BODY_SKILL_SCRATCH, "scratch", 5u, 140u, 4u, 7u, 1u, 220u,
     {140u, 124u, 208u, 96u, 116u, 152u, 176u, 192u, 128u, 148u, 112u, 156u, 220u, 88u, 184u, 136u}},
    {CAZ_BODY_SKILL_GROOM, "groom", 0u, 96u, 0u, 1u, 3u, 132u,
     {96u, 148u, 180u, 108u, 108u, 108u, 88u, 120u, 78u, 150u, 90u, 90u, 196u, 96u, 110u, 116u}},
    {CAZ_BODY_SKILL_STRETCH, "stretch", 2u, 128u, 2u, 6u, 0u, 204u,
     {128u, 140u, 68u, 68u, 188u, 188u, 196u, 228u, 150u, 176u, 148u, 148u, 72u, 72u, 170u, 128u}},
    {CAZ_BODY_SKILL_STARTLE, "startle", 4u, 92u, 3u, 8u, 4u, 248u,
     {92u, 120u, 96u, 96u, 168u, 168u, 220u, 244u, 120u, 112u, 160u, 160u, 96u, 96u, 188u, 112u}},
    {CAZ_BODY_SKILL_RECOVER, "recover", 0u, 128u, 3u, 8u, 4u, 255u,
     {128u, 128u, 128u, 128u, 128u, 128u, 220u, 240u, 100u, 128u, 128u, 128u, 128u, 128u, 180u, 128u}}
};

#define CAZ_BODY_SKILL_TABLE_COUNT (sizeof(built_in_skill_table) / sizeof(built_in_skill_table[0]))

static CazBodySkillDef skill_table[CAZ_BODY_SKILL_TABLE_COUNT];
static bool skill_library_ready = false;

static uint8_t joint_index(uint8_t value)
{
    return (uint8_t)(value % CAZ_BODY_JOINT_COUNT);
}

static uint8_t pose_frame_index(uint8_t value)
{
    return (uint8_t)(value % CAZ_BODY_POSE_FRAME_COUNT);
}

static uint8_t clamp_joint_value(uint8_t index, uint8_t value)
{
    const CazBodyJointLimit *limit = &joint_limits[joint_index(index)];
    if (value < limit->minimum) {
        return limit->minimum;
    }
    if (value > limit->maximum) {
        return limit->maximum;
    }
    return value;
}

static const CazBodySkillDef *find_skill(uint8_t skill)
{
    size_t i;
    for (i = 0u; i < CAZ_BODY_SKILL_TABLE_COUNT; i++) {
        if (skill_table[i].skill == skill) {
            return &skill_table[i];
        }
    }
    return NULL;
}

static CazBodySkillDef *find_mutable_skill(uint8_t skill)
{
    size_t i;
    for (i = 0u; i < CAZ_BODY_SKILL_TABLE_COUNT; i++) {
        if (skill_table[i].skill == skill) {
            return &skill_table[i];
        }
    }
    return NULL;
}

void caz_body_reset_skill_library(void)
{
    memcpy(skill_table, built_in_skill_table, sizeof(skill_table));
    skill_library_ready = true;
}

static void ensure_skill_library(void)
{
    if (!skill_library_ready) {
        caz_body_reset_skill_library();
    }
}

static uint8_t skill_for_gait(uint8_t gait)
{
    switch (gait % 6u) {
    case 0: return CAZ_BODY_SKILL_REST;
    case 1: return CAZ_BODY_SKILL_WALK;
    case 2: return CAZ_BODY_SKILL_CRAWL;
    case 3: return CAZ_BODY_SKILL_POUNCE;
    case 4: return CAZ_BODY_SKILL_STARTLE;
    case 5: return CAZ_BODY_SKILL_SNIFF;
    default: return CAZ_BODY_SKILL_NONE;
    }
}

static void copy_joints(uint8_t *dst, const uint8_t *src)
{
    size_t i;
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        dst[i] = clamp_joint_value((uint8_t)i, src[i]);
    }
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
            if (ch == '_') {
                dst[i++] = '-';
            } else {
                dst[i++] = (char)tolower(ch);
            }
        }
        src++;
    }
    dst[i] = '\0';
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

static bool parse_byte(const char *text, uint8_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 0);
    if (end == text || *trim_text(end) != '\0' || parsed > 255ul) {
        return false;
    }
    *value = (uint8_t)parsed;
    return true;
}

static bool skill_from_text(const char *text, uint8_t *skill)
{
    char key[64];
    normalize_key(key, sizeof(key), text);
    if (strncmp(key, "skill-", 6u) == 0) {
        memmove(key, key + 6, strlen(key + 6) + 1u);
    }
    if (strcmp(key, "none") == 0) {
        *skill = CAZ_BODY_SKILL_NONE;
        return true;
    }
    if (strcmp(key, "balance") == 0) { *skill = CAZ_BODY_SKILL_BALANCE; return true; }
    if (strcmp(key, "rest") == 0) { *skill = CAZ_BODY_SKILL_REST; return true; }
    if (strcmp(key, "sit") == 0) { *skill = CAZ_BODY_SKILL_SIT; return true; }
    if (strcmp(key, "walk") == 0) { *skill = CAZ_BODY_SKILL_WALK; return true; }
    if (strcmp(key, "crawl") == 0) { *skill = CAZ_BODY_SKILL_CRAWL; return true; }
    if (strcmp(key, "pounce") == 0) { *skill = CAZ_BODY_SKILL_POUNCE; return true; }
    if (strcmp(key, "sniff") == 0) { *skill = CAZ_BODY_SKILL_SNIFF; return true; }
    if (strcmp(key, "scratch") == 0) { *skill = CAZ_BODY_SKILL_SCRATCH; return true; }
    if (strcmp(key, "groom") == 0) { *skill = CAZ_BODY_SKILL_GROOM; return true; }
    if (strcmp(key, "stretch") == 0) { *skill = CAZ_BODY_SKILL_STRETCH; return true; }
    if (strcmp(key, "startle") == 0) { *skill = CAZ_BODY_SKILL_STARTLE; return true; }
    if (strcmp(key, "recover") == 0) { *skill = CAZ_BODY_SKILL_RECOVER; return true; }
    return parse_byte(text, skill);
}

static bool joint_from_key(const char *key, uint8_t *joint)
{
    size_t i;
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        char joint_key[64];
        normalize_key(joint_key, sizeof(joint_key), joint_limits[i].name);
        if (strcmp(key, joint_key) == 0) {
            *joint = (uint8_t)i;
            return true;
        }
    }
    return false;
}

static bool has_suffix(const char *text, const char *suffix)
{
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}

static bool infer_skill_from_path(const char *path, uint8_t *skill)
{
    char name[128];
    const char *base = strrchr(path, '/');
    char *dot;
    base = base == NULL ? path : base + 1;
    snprintf(name, sizeof(name), "%s", base);
    dot = strrchr(name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    return skill_from_text(name, skill);
}

static void skill_error(char *error, size_t error_length, const char *path, int line_number, const char *message)
{
    if (error_length == 0u) {
        return;
    }
    if (line_number > 0) {
        snprintf(error, error_length, "%s:%d: %s", path, line_number, message);
    } else {
        snprintf(error, error_length, "%s: %s", path, message);
    }
}

static bool is_frame_section(const char *text)
{
    char section[64];
    size_t length = strlen(text);
    if (length < 3u || text[0] != '[' || text[length - 1u] != ']') {
        return false;
    }
    snprintf(section, sizeof(section), "%.*s", (int)(length - 2u), text + 1);
    normalize_key(section, sizeof(section), trim_text(section));
    return strcmp(section, "frame") == 0 ||
           strncmp(section, "frame-", 6u) == 0;
}

static bool parse_bool_text(const char *text, bool *value)
{
    char normalized[16];
    normalize_key(normalized, sizeof(normalized), text);
    if (strcmp(normalized, "true") == 0 ||
        strcmp(normalized, "yes") == 0 ||
        strcmp(normalized, "1") == 0) {
        *value = true;
        return true;
    }
    if (strcmp(normalized, "false") == 0 ||
        strcmp(normalized, "no") == 0 ||
        strcmp(normalized, "0") == 0) {
        *value = false;
        return true;
    }
    return false;
}

static bool parse_joint_list(const char *text, uint8_t *joints)
{
    char buffer[256];
    char *cursor;
    size_t index = 0u;
    snprintf(buffer, sizeof(buffer), "%s", text);
    cursor = buffer;
    while (cursor != NULL && index < CAZ_BODY_JOINT_COUNT) {
        char *comma = strchr(cursor, ',');
        uint8_t value;
        if (comma != NULL) {
            *comma = '\0';
        }
        if (!parse_byte(trim_text(cursor), &value)) {
            return false;
        }
        joints[index] = clamp_joint_value((uint8_t)index, value);
        index++;
        cursor = comma == NULL ? NULL : comma + 1;
    }
    return cursor == NULL && index == CAZ_BODY_JOINT_COUNT;
}

static bool load_skill_file(const char *path, char *error, size_t error_length)
{
    FILE *file = fopen(path, "rb");
    CazBodySkillDef parsed;
    CazBodySkillDef *target;
    char line[256];
    int line_number = 0;
    bool in_frame = false;

    if (file == NULL) {
        skill_error(error, error_length, path, 0, "could not open skill file");
        return false;
    }

    if (!infer_skill_from_path(path, &parsed.skill)) {
        fclose(file);
        skill_error(error, error_length, path, 0, "could not infer skill name from file name");
        return false;
    }

    target = find_mutable_skill(parsed.skill);
    if (target == NULL) {
        fclose(file);
        skill_error(error, error_length, path, 0, "unknown skill");
        return false;
    }
    parsed = *target;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *text;
        char *equals;
        char key[64];
        uint8_t value;
        line_number++;
        text = trim_text(line);
        if (*text == '\0' || *text == ';' || *text == '#') {
            continue;
        }
        if (is_frame_section(text)) {
            in_frame = true;
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL) {
            fclose(file);
            skill_error(error, error_length, path, line_number, "expected key=value");
            return false;
        }
        *equals = '\0';
        normalize_key(key, sizeof(key), trim_text(text));
        text = trim_text(equals + 1);

        if (!in_frame && strcmp(key, "skill") == 0) {
            if (!skill_from_text(text, &parsed.skill)) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "unknown skill name");
                return false;
            }
            target = find_mutable_skill(parsed.skill);
            if (target == NULL) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "skill is not built in");
                return false;
            }
            parsed = *target;
        } else if (!in_frame && strcmp(key, "name") == 0) {
            continue;
        } else if (!in_frame && strcmp(key, "id") == 0) {
            if (!parse_byte(text, &value)) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "invalid skill id");
                return false;
            }
            if (find_mutable_skill(value) == NULL) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "skill id is not built in");
                return false;
            }
            parsed.skill = value;
        } else if (!in_frame && strcmp(key, "duration") == 0) {
            if (!parse_byte(text, &value)) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "invalid duration");
                return false;
            }
        } else if (!in_frame && strcmp(key, "interruptible") == 0) {
            bool ignored_interruptible;
            if (!parse_bool_text(text, &ignored_interruptible)) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "invalid interruptible flag");
                return false;
            }
        } else if (!in_frame && parse_byte(text, &value)) {
            if (strcmp(key, "gait") == 0) parsed.gait = (uint8_t)(value % 6u);
            else if (strcmp(key, "head-yaw") == 0) parsed.head_yaw = value;
            else if (strcmp(key, "ear-pose") == 0) parsed.ear_pose = (uint8_t)(value % 5u);
            else if (strcmp(key, "tail-pose") == 0) parsed.tail_pose = (uint8_t)(value % 9u);
            else if (strcmp(key, "vocal") == 0) parsed.vocal = (uint8_t)(value % 6u);
            else if (strcmp(key, "eyelid") == 0) parsed.eyelid = value;
            else {
                fclose(file);
                skill_error(error, error_length, path, line_number, "unknown metadata key");
                return false;
            }
        } else if (in_frame && parse_byte(text, &value)) {
            uint8_t joint;
            if (!joint_from_key(key, &joint)) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "unknown frame joint");
                return false;
            }
            parsed.joints[joint] = clamp_joint_value(joint, value);
        } else if (in_frame && strcmp(key, "joints") == 0) {
            if (!parse_joint_list(text, parsed.joints)) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "expected 16 comma-separated joint values");
                return false;
            }
        } else if (in_frame && strcmp(key, "time") == 0) {
            if (!parse_byte(text, &value)) {
                fclose(file);
                skill_error(error, error_length, path, line_number, "invalid frame time");
                return false;
            }
        } else {
            fclose(file);
            skill_error(error, error_length, path, line_number, "invalid byte value");
            return false;
        }
    }

    fclose(file);
    target = find_mutable_skill(parsed.skill);
    if (target == NULL) {
        skill_error(error, error_length, path, 0, "unknown skill");
        return false;
    }
    *target = parsed;
    return true;
}

bool caz_body_load_skill_dir(const char *path, char *error, size_t error_length)
{
    DIR *dir;
    struct dirent *entry;
    caz_body_reset_skill_library();
    dir = opendir(path);
    if (dir == NULL) {
        if (errno == ENOENT) {
            return true;
        }
        skill_error(error, error_length, path, 0, "could not open skill directory");
        return false;
    }
    while ((entry = readdir(dir)) != NULL) {
        char full_path[512];
        if (entry->d_name[0] == '.' || !has_suffix(entry->d_name, ".cazskill")) {
            continue;
        }
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (!load_skill_file(full_path, error, error_length)) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}

static void set_requested_skill_targets(CazBody *body, uint8_t skill, bool copy_pose)
{
    const CazBodySkillDef *def = find_skill(skill);
    body->requested_skill = skill;
    if (def == NULL) {
        return;
    }
    copy_joints(body->requested_joint_targets, def->joints);
    if (copy_pose) {
        body->requested_gait = def->gait;
        body->requested_head_yaw = def->head_yaw;
        body->requested_ear_pose = def->ear_pose;
        body->requested_tail_pose = def->tail_pose;
        body->requested_vocal = def->vocal;
        body->requested_eyelid = def->eyelid;
    }
}

static void copy_requested_pose(CazBody *body)
{
    copy_joints(body->joint_targets, body->requested_joint_targets);
    body->gait = body->requested_gait;
    body->head_yaw = body->requested_head_yaw;
    body->ear_pose = body->requested_ear_pose;
    body->tail_pose = body->requested_tail_pose;
    body->vocal = body->requested_vocal;
    body->eyelid = body->requested_eyelid;
    body->active_skill = body->requested_skill;
}

static void mark_reflex(CazBody *body, uint8_t active_skill, CazBodyReflexState reflex_state)
{
    const CazBodySkillDef *def = find_skill(active_skill);
    if (def != NULL) {
        copy_joints(body->joint_targets, def->joints);
    }
    body->active_skill = active_skill;
    body->skill_status = CAZ_BODY_SKILL_REFLEX;
    body->reflex_state = reflex_state;
}

static bool gait_is_unsafe(uint8_t gait)
{
    switch (gait % 6u) {
    case 1:
    case 3:
    case 4:
    case 5:
        return true;
    default:
        return false;
    }
}

static bool body_is_tilted(const CazBody *body)
{
    return body->imu_roll < 96u ||
           body->imu_roll > 160u ||
           body->imu_pitch < 96u ||
           body->imu_pitch > 160u;
}

static void normalize_status(CazBody *body)
{
    body->reflex_state = CAZ_BODY_REFLEX_CLEAR;
    if (body->active_skill == CAZ_BODY_SKILL_NONE) {
        body->skill_status = CAZ_BODY_SKILL_IDLE;
    } else if (body->skill_status == CAZ_BODY_SKILL_REFLEX ||
               body->skill_status == CAZ_BODY_SKILL_IDLE) {
        body->skill_status = CAZ_BODY_SKILL_READY;
    }
}

static uint8_t blend_value(uint8_t current, uint8_t target, uint8_t step)
{
    if (current < target) {
        uint8_t delta = (uint8_t)(target - current);
        return delta <= step ? target : (uint8_t)(current + step);
    }
    if (current > target) {
        uint8_t delta = (uint8_t)(current - target);
        return delta <= step ? target : (uint8_t)(current - step);
    }
    return current;
}

void caz_body_init(CazBody *body)
{
    size_t i;
    ensure_skill_library();
    memset(body, 0, sizeof(*body));
    body->imu_roll = 128u;
    body->imu_pitch = 128u;
    body->battery = 255u;
    body->requested_head_yaw = 128u;
    body->requested_ear_pose = 1u;
    body->requested_tail_pose = 2u;
    body->requested_eyelid = 180u;
    body->requested_skill = CAZ_BODY_SKILL_NONE;
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        body->requested_joint_targets[i] = 128u;
        body->joint_targets[i] = 128u;
        body->joint_values[i] = 128u;
        body->pose_frame_buffer[i] = 128u;
        body->pose_frame[i] = 128u;
    }
    copy_requested_pose(body);
    body->skill_status = CAZ_BODY_SKILL_IDLE;
    body->reflex_state = CAZ_BODY_REFLEX_CLEAR;
}

void caz_body_set_normalized_sensors(CazBody *body,
                                     uint8_t imu_roll,
                                     uint8_t imu_pitch,
                                     uint8_t lifted,
                                     uint8_t dropped,
                                     uint8_t battery,
                                     uint8_t terrain)
{
    body->imu_roll = imu_roll;
    body->imu_pitch = imu_pitch;
    body->lifted = lifted;
    body->dropped = dropped;
    body->battery = battery;
    body->terrain = terrain;
}

void caz_body_apply_reflexes(CazBody *body)
{
    copy_requested_pose(body);
    normalize_status(body);

    if (body->battery < 32u && gait_is_unsafe(body->requested_gait)) {
        body->gait = 0u;
        body->head_yaw = 128u;
        body->ear_pose = 1u;
        body->tail_pose = 2u;
        body->vocal = 1u;
        body->eyelid = 96u;
        mark_reflex(body, CAZ_BODY_SKILL_REST, CAZ_BODY_REFLEX_LOW_BATTERY);
    } else if (body->dropped != 0u) {
        body->gait = 0u;
        body->head_yaw = 128u;
        body->ear_pose = 3u;
        body->tail_pose = 8u;
        body->vocal = 4u;
        body->eyelid = 255u;
        mark_reflex(body, CAZ_BODY_SKILL_RECOVER, CAZ_BODY_REFLEX_DROPPED);
    } else if (body->lifted != 0u) {
        body->gait = 0u;
        body->head_yaw = 128u;
        body->ear_pose = 3u;
        body->tail_pose = 2u;
        body->vocal = 1u;
        body->eyelid = 220u;
        mark_reflex(body, CAZ_BODY_SKILL_RECOVER, CAZ_BODY_REFLEX_LIFTED);
    } else if (body_is_tilted(body)) {
        body->gait = 2u;
        body->head_yaw = 128u;
        body->ear_pose = 2u;
        body->tail_pose = 5u;
        body->vocal = 0u;
        body->eyelid = 220u;
        mark_reflex(body, CAZ_BODY_SKILL_BALANCE, CAZ_BODY_REFLEX_BALANCE);
    } else if (body->terrain >= 4u && (body->requested_gait == 3u || body->requested_gait == 4u)) {
        body->gait = 2u;
        body->head_yaw = body->requested_head_yaw;
        body->ear_pose = 2u;
        body->tail_pose = 3u;
        body->vocal = 0u;
        body->eyelid = body->requested_eyelid;
        mark_reflex(body, CAZ_BODY_SKILL_CRAWL, CAZ_BODY_REFLEX_TERRAIN_CAUTION);
    }
}

void caz_body_tick(CazBody *body)
{
    size_t i;
    uint8_t step = body->pose_frame_time == 0u ? 8u : (uint8_t)(64u / body->pose_frame_time);
    if (step == 0u) {
        step = 1u;
    }

    caz_body_apply_reflexes(body);
    if (body->active_skill != CAZ_BODY_SKILL_NONE && body->skill_status != CAZ_BODY_SKILL_REFLEX) {
        body->skill_status = CAZ_BODY_SKILL_RUNNING;
    }
    for (i = 0u; i < CAZ_BODY_JOINT_COUNT; i++) {
        body->joint_values[i] = blend_value(body->joint_values[i], body->joint_targets[i], step);
    }
}

bool caz_body_read_port(const CazBody *body, uint8_t port, uint8_t *value)
{
    switch (port) {
    case CAZ_PORT_IMU_ROLL:
        *value = body->imu_roll;
        return true;
    case CAZ_PORT_IMU_PITCH:
        *value = body->imu_pitch;
        return true;
    case CAZ_PORT_LIFTED:
        *value = body->lifted;
        return true;
    case CAZ_PORT_DROPPED:
        *value = body->dropped;
        return true;
    case CAZ_PORT_BATTERY:
        *value = body->battery;
        return true;
    case CAZ_PORT_TERRAIN:
        *value = body->terrain;
        return true;
    case CAZ_PORT_SKILL:
        *value = body->active_skill;
        return true;
    case CAZ_PORT_SKILL_ARG:
        *value = body->skill_arg;
        return true;
    case CAZ_PORT_SKILL_STATUS:
        *value = body->skill_status;
        return true;
    case CAZ_PORT_REFLEX_STATE:
        *value = body->reflex_state;
        return true;
    case CAZ_PORT_JOINT_INDEX:
        *value = body->selected_joint;
        return true;
    case CAZ_PORT_JOINT_ANGLE:
        *value = body->joint_targets[body->selected_joint];
        return true;
    case CAZ_PORT_POSE_FRAME_INDEX:
        *value = body->pose_frame_index;
        return true;
    case CAZ_PORT_POSE_FRAME_VALUE:
        *value = body->pose_frame[body->pose_frame_index];
        return true;
    case CAZ_PORT_POSE_FRAME_FLAGS:
        *value = body->pose_frame_flags;
        return true;
    case CAZ_PORT_POSE_FRAME_TIME:
        *value = body->pose_frame_time;
        return true;
    default:
        return false;
    }
}

bool caz_body_write_port(CazBody *body, uint8_t port, uint8_t value)
{
    switch (port) {
    case CAZ_PORT_GAIT:
        body->requested_gait = (uint8_t)(value % 6u);
        set_requested_skill_targets(body, skill_for_gait(value), false);
        body->skill_status = CAZ_BODY_SKILL_READY;
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_HEAD_YAW:
        body->requested_head_yaw = value;
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_EAR_POSE:
        body->requested_ear_pose = (uint8_t)(value % 5u);
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_TAIL_POSE:
        body->requested_tail_pose = (uint8_t)(value % 9u);
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_VOCAL:
        body->requested_vocal = (uint8_t)(value % 6u);
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_EYELID:
        body->requested_eyelid = value;
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_SKILL:
        set_requested_skill_targets(body, value, true);
        body->skill_status = value == CAZ_BODY_SKILL_NONE ? CAZ_BODY_SKILL_IDLE : CAZ_BODY_SKILL_READY;
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_SKILL_ARG:
        body->skill_arg = value;
        return true;
    case CAZ_PORT_SKILL_STATUS:
        body->skill_status = value;
        return true;
    case CAZ_PORT_REFLEX_STATE:
        body->reflex_state = value;
        return true;
    case CAZ_PORT_JOINT_INDEX:
        body->selected_joint = joint_index(value);
        return true;
    case CAZ_PORT_JOINT_ANGLE:
        body->pending_joint_angle = clamp_joint_value(body->selected_joint, value);
        return true;
    case CAZ_PORT_JOINT_COMMIT:
        body->requested_joint_targets[body->selected_joint] = body->pending_joint_angle;
        caz_body_apply_reflexes(body);
        return true;
    case CAZ_PORT_POSE_FRAME_INDEX:
        body->pose_frame_index = pose_frame_index(value);
        return true;
    case CAZ_PORT_POSE_FRAME_VALUE:
        body->pose_frame_value = clamp_joint_value(body->pose_frame_index, value);
        body->pose_frame_buffer[body->pose_frame_index] = body->pose_frame_value;
        return true;
    case CAZ_PORT_POSE_FRAME_FLAGS:
        body->pose_frame_flags = value;
        return true;
    case CAZ_PORT_POSE_FRAME_TIME:
        body->pose_frame_time = value;
        return true;
    case CAZ_PORT_POSE_FRAME_COMMIT:
        copy_joints(body->pose_frame, body->pose_frame_buffer);
        copy_joints(body->requested_joint_targets, body->pose_frame);
        caz_body_apply_reflexes(body);
        return true;
    default:
        return false;
    }
}

uint8_t caz_body_joint_value(const CazBody *body, uint8_t joint)
{
    return body->joint_values[joint_index(joint)];
}

uint8_t caz_body_joint_target(const CazBody *body, uint8_t joint)
{
    return body->joint_targets[joint_index(joint)];
}

const char *caz_body_skill_name(uint8_t skill)
{
    const CazBodySkillDef *def;
    if (skill == CAZ_BODY_SKILL_NONE) {
        return "none";
    }
    def = find_skill(skill);
    return def != NULL ? def->name : "unknown";
}

const char *caz_body_skill_status_name(uint8_t status)
{
    switch (status) {
    case CAZ_BODY_SKILL_IDLE: return "idle";
    case CAZ_BODY_SKILL_READY: return "ready";
    case CAZ_BODY_SKILL_RUNNING: return "running";
    case CAZ_BODY_SKILL_BLOCKED: return "blocked";
    case CAZ_BODY_SKILL_REFLEX: return "reflex";
    default: return "unknown";
    }
}

const char *caz_body_joint_name(uint8_t joint)
{
    if (joint >= CAZ_BODY_JOINT_COUNT) {
        return "unknown";
    }
    return joint_limits[joint].name;
}

const char *caz_body_reflex_state_name(uint8_t reflex_state)
{
    switch (reflex_state) {
    case CAZ_BODY_REFLEX_CLEAR: return "clear";
    case CAZ_BODY_REFLEX_LOW_BATTERY: return "low-battery";
    case CAZ_BODY_REFLEX_DROPPED: return "dropped";
    case CAZ_BODY_REFLEX_LIFTED: return "lifted";
    case CAZ_BODY_REFLEX_BALANCE: return "balance";
    case CAZ_BODY_REFLEX_TERRAIN_CAUTION: return "terrain";
    default: return "unknown";
    }
}
