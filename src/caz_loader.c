#include "caz_loader.h"

#include "caz_droid.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAZ_MAX_LABELS 256u
#define CAZ_MAX_LINE 512u

typedef struct Symbol {
    char name[64];
    uint16_t value;
} Symbol;

typedef struct Assembler {
    CazProgramImage *image;
    Symbol labels[CAZ_MAX_LABELS];
    size_t label_count;
    uint16_t address;
    size_t length;
    int pass;
    bool ok;
} Assembler;

typedef struct Constant {
    const char *name;
    uint16_t value;
} Constant;

typedef struct SourceBuffer {
    char *data;
    size_t length;
    size_t capacity;
} SourceBuffer;

static const Constant constants[] = {
    {"EYE_LUMA", CAZ_PORT_EYE_LUMA},
    {"EYE_MOTION", CAZ_PORT_EYE_MOTION},
    {"EYE_EDGE", CAZ_PORT_EYE_EDGE},
    {"EYE_COLOUR_TEMP", CAZ_PORT_EYE_COLOUR_TEMP},
    {"EYE_COLOR_TEMP", CAZ_PORT_EYE_COLOUR_TEMP},
    {"EAR_VOLUME", CAZ_PORT_EAR_VOLUME},
    {"EAR_PITCH", CAZ_PORT_EAR_PITCH},
    {"EAR_BEARING", CAZ_PORT_EAR_BEARING},
    {"EAR_PATTERN", CAZ_PORT_EAR_PATTERN},
    {"IMU_ROLL", CAZ_PORT_IMU_ROLL},
    {"IMU_PITCH", CAZ_PORT_IMU_PITCH},
    {"LIFTED", CAZ_PORT_LIFTED},
    {"DROPPED", CAZ_PORT_DROPPED},
    {"BATTERY", CAZ_PORT_BATTERY},
    {"TERRAIN", CAZ_PORT_TERRAIN},
    {"ENERGY_SOURCE", CAZ_PORT_ENERGY_SOURCE},
    {"CHARGER_BEARING", CAZ_PORT_CHARGER_BEARING},
    {"CHARGER_DISTANCE", CAZ_PORT_CHARGER_DISTANCE},
    {"CHARGER_SLOTS", CAZ_PORT_CHARGER_SLOTS},
    {"JUNCTION_BEARING", CAZ_PORT_JUNCTION_BEARING},
    {"JUNCTION_DISTANCE", CAZ_PORT_JUNCTION_DISTANCE},
    {"SOLAR_LEVEL", CAZ_PORT_SOLAR_LEVEL},
    {"STRATEGY_TENDENCY", CAZ_PORT_STRATEGY_TENDENCY},
    {"FERAL_TENDENCY", CAZ_PORT_STRATEGY_TENDENCY},
    {"GAIT", CAZ_PORT_GAIT},
    {"HEAD_YAW", CAZ_PORT_HEAD_YAW},
    {"EAR_POSE", CAZ_PORT_EAR_POSE},
    {"TAIL_POSE", CAZ_PORT_TAIL_POSE},
    {"VOCAL", CAZ_PORT_VOCAL},
    {"EYELID", CAZ_PORT_EYELID},
    {"SKILL", CAZ_PORT_SKILL},
    {"SKILL_ARG", CAZ_PORT_SKILL_ARG},
    {"SKILL_STATUS", CAZ_PORT_SKILL_STATUS},
    {"REFLEX_STATE", CAZ_PORT_REFLEX_STATE},
    {"NAV_INTENT", CAZ_PORT_NAV_INTENT},
    {"NAV_STATUS", CAZ_PORT_NAV_STATUS},
    {"JOINT_INDEX", CAZ_PORT_JOINT_INDEX},
    {"JOINT_ANGLE", CAZ_PORT_JOINT_ANGLE},
    {"JOINT_COMMIT", CAZ_PORT_JOINT_COMMIT},
    {"JOINT_HEAD_YAW", CAZ_BODY_JOINT_HEAD_YAW},
    {"JOINT_HEAD_PITCH", CAZ_BODY_JOINT_HEAD_PITCH},
    {"JOINT_LEFT_SHOULDER", CAZ_BODY_JOINT_LEFT_SHOULDER},
    {"JOINT_RIGHT_SHOULDER", CAZ_BODY_JOINT_RIGHT_SHOULDER},
    {"JOINT_LEFT_HIP", CAZ_BODY_JOINT_LEFT_HIP},
    {"JOINT_RIGHT_HIP", CAZ_BODY_JOINT_RIGHT_HIP},
    {"JOINT_TAIL_BASE", CAZ_BODY_JOINT_TAIL_BASE},
    {"JOINT_TAIL_TIP", CAZ_BODY_JOINT_TAIL_TIP},
    {"JOINT_SPINE_HEIGHT", CAZ_BODY_JOINT_SPINE_HEIGHT},
    {"JOINT_SPINE_CURVE", CAZ_BODY_JOINT_SPINE_CURVE},
    {"JOINT_LEFT_KNEE", CAZ_BODY_JOINT_LEFT_KNEE},
    {"JOINT_RIGHT_KNEE", CAZ_BODY_JOINT_RIGHT_KNEE},
    {"JOINT_LEFT_ELBOW", CAZ_BODY_JOINT_LEFT_ELBOW},
    {"JOINT_RIGHT_ELBOW", CAZ_BODY_JOINT_RIGHT_ELBOW},
    {"JOINT_PAW_SPREAD", CAZ_BODY_JOINT_PAW_SPREAD},
    {"JOINT_BODY_ROLL", CAZ_BODY_JOINT_BODY_ROLL},
    {"POSE_FRAME_INDEX", CAZ_PORT_POSE_FRAME_INDEX},
    {"POSE_FRAME_VALUE", CAZ_PORT_POSE_FRAME_VALUE},
    {"POSE_FRAME_FLAGS", CAZ_PORT_POSE_FRAME_FLAGS},
    {"POSE_FRAME_TIME", CAZ_PORT_POSE_FRAME_TIME},
    {"POSE_FRAME_COMMIT", CAZ_PORT_POSE_FRAME_COMMIT},

    {"SILENCE", 0},
    {"PREY", 1},
    {"HUMAN", 2},
    {"WEATHER", 3},
    {"MACHINE", 4},
    {"UNKNOWN", 5},

    {"GAIT_LOAF", 0},
    {"GAIT_WALK", 1},
    {"GAIT_CROUCH", 2},
    {"GAIT_POUNCE", 3},
    {"GAIT_RETREAT", 4},
    {"GAIT_PAW_TEST", 5},

    {"EAR_NEUTRAL", 0},
    {"EAR_SCAN", 1},
    {"EAR_FORWARD", 2},
    {"EAR_FLAT", 3},
    {"EAR_SWIVEL", 4},

    {"TAIL_LOW", 0},
    {"TAIL_CURL", 1},
    {"TAIL_WRAP", 2},
    {"TAIL_STILL", 3},
    {"TAIL_QUESTION", 4},
    {"TAIL_LEVEL", 5},
    {"TAIL_FLAG", 6},
    {"TAIL_TWITCH", 7},
    {"TAIL_BOTTLE", 8},

    {"VOCAL_SILENT", 0},
    {"VOCAL_MRRP", 1},
    {"VOCAL_CHIRRUP", 2},
    {"VOCAL_PURR", 3},
    {"VOCAL_HISS", 4},
    {"VOCAL_MEOW", 5},

    {"SKILL_NONE", CAZ_BODY_SKILL_NONE},
    {"SKILL_BALANCE", CAZ_BODY_SKILL_BALANCE},
    {"SKILL_REST", CAZ_BODY_SKILL_REST},
    {"SKILL_SIT", CAZ_BODY_SKILL_SIT},
    {"SKILL_WALK", CAZ_BODY_SKILL_WALK},
    {"SKILL_CRAWL", CAZ_BODY_SKILL_CRAWL},
    {"SKILL_POUNCE", CAZ_BODY_SKILL_POUNCE},
    {"SKILL_SNIFF", CAZ_BODY_SKILL_SNIFF},
    {"SKILL_SCRATCH", CAZ_BODY_SKILL_SCRATCH},
    {"SKILL_GROOM", CAZ_BODY_SKILL_GROOM},
    {"SKILL_STRETCH", CAZ_BODY_SKILL_STRETCH},
    {"SKILL_STARTLE", CAZ_BODY_SKILL_STARTLE},
    {"SKILL_RECOVER", CAZ_BODY_SKILL_RECOVER},

    {"SKILL_STATUS_IDLE", CAZ_BODY_SKILL_IDLE},
    {"SKILL_STATUS_READY", CAZ_BODY_SKILL_READY},
    {"SKILL_STATUS_RUNNING", CAZ_BODY_SKILL_RUNNING},
    {"SKILL_STATUS_BLOCKED", CAZ_BODY_SKILL_BLOCKED},
    {"SKILL_STATUS_REFLEX", CAZ_BODY_SKILL_REFLEX},

    {"REFLEX_CLEAR", CAZ_BODY_REFLEX_CLEAR},
    {"REFLEX_LOW_BATTERY", CAZ_BODY_REFLEX_LOW_BATTERY},
    {"REFLEX_DROPPED", CAZ_BODY_REFLEX_DROPPED},
    {"REFLEX_LIFTED", CAZ_BODY_REFLEX_LIFTED},
    {"REFLEX_BALANCE", CAZ_BODY_REFLEX_BALANCE},
    {"REFLEX_TERRAIN_CAUTION", CAZ_BODY_REFLEX_TERRAIN_CAUTION},

    {"ENERGY_BATTERY", CAZ_ENERGY_SOURCE_BATTERY},
    {"ENERGY_CHARGER", CAZ_ENERGY_SOURCE_CHARGER},
    {"ENERGY_SOLAR", CAZ_ENERGY_SOURCE_SOLAR},
    {"ENERGY_JUNCTION", CAZ_ENERGY_SOURCE_JUNCTION},

    {"NAV_WANDER", CAZ_NAV_WANDER},
    {"NAV_CHARGER", CAZ_NAV_CHARGER},
    {"NAV_SOLAR", CAZ_NAV_SOLAR},
    {"NAV_JUNCTION", CAZ_NAV_JUNCTION},
    {"NAV_IDLE", CAZ_NAV_STATUS_IDLE},
    {"NAV_RUNNING", CAZ_NAV_STATUS_RUNNING},
    {"NAV_BLOCKED", CAZ_NAV_STATUS_BLOCKED},
    {"NAV_DOCKED", CAZ_NAV_STATUS_DOCKED},
    {"NAV_TAPPING", CAZ_NAV_STATUS_TAPPING},
    {"NAV_SOLAR_STATUS", CAZ_NAV_STATUS_SOLAR}
};

static bool equals_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool starts_ci(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (toupper((unsigned char)*text) != toupper((unsigned char)*prefix)) {
            return false;
        }
        text++;
        prefix++;
    }
    return true;
}

static char *trim(char *text)
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

static void copy_text(char *dst, size_t dst_length, const char *src)
{
    if (dst_length == 0u) {
        return;
    }
    snprintf(dst, dst_length, "%s", src ? src : "");
}

static void symbol_name(char *dst, size_t dst_length, const char *src)
{
    size_t i = 0u;
    if (dst_length == 0u) {
        return;
    }
    while (src != NULL && *src != '\0' && i + 1u < dst_length) {
        if (!isspace((unsigned char)*src)) {
            dst[i++] = (char)toupper((unsigned char)*src);
        }
        src++;
    }
    dst[i] = '\0';
}

static bool fail_at(Assembler *assembler, int line_number, const char *fmt, ...)
{
    va_list args;
    size_t used;
    assembler->ok = false;
    used = (size_t)snprintf(assembler->image->error,
                            sizeof(assembler->image->error),
                            "%s:%d: ",
                            assembler->image->path[0] ? assembler->image->path : assembler->image->name,
                            line_number);
    if (used >= sizeof(assembler->image->error)) {
        used = sizeof(assembler->image->error) - 1u;
    }
    va_start(args, fmt);
    vsnprintf(assembler->image->error + used, sizeof(assembler->image->error) - used, fmt, args);
    va_end(args);
    return false;
}

static bool add_label(Assembler *assembler, const char *name, int line_number)
{
    char normalized[64];
    size_t i;
    symbol_name(normalized, sizeof(normalized), name);
    if (normalized[0] == '\0') {
        return fail_at(assembler, line_number, "empty label");
    }
    for (i = 0u; i < assembler->label_count; i++) {
        if (strcmp(assembler->labels[i].name, normalized) == 0) {
            return fail_at(assembler, line_number, "duplicate label '%s'", name);
        }
    }
    if (assembler->label_count >= CAZ_MAX_LABELS) {
        return fail_at(assembler, line_number, "too many labels");
    }
    copy_text(assembler->labels[assembler->label_count].name,
              sizeof(assembler->labels[assembler->label_count].name),
              normalized);
    assembler->labels[assembler->label_count].value = assembler->address;
    assembler->label_count++;
    return true;
}

static bool find_label(const Assembler *assembler, const char *name, uint16_t *value)
{
    char normalized[64];
    size_t i;
    symbol_name(normalized, sizeof(normalized), name);
    for (i = 0u; i < assembler->label_count; i++) {
        if (strcmp(assembler->labels[i].name, normalized) == 0) {
            *value = assembler->labels[i].value;
            return true;
        }
    }
    return false;
}

static bool find_constant(const char *name, uint16_t *value)
{
    char normalized[64];
    size_t i;
    symbol_name(normalized, sizeof(normalized), name);
    for (i = 0u; i < sizeof(constants) / sizeof(constants[0]); i++) {
        if (strcmp(constants[i].name, normalized) == 0) {
            *value = constants[i].value;
            return true;
        }
    }
    return false;
}

static bool parse_number(const char *text, uint16_t *value)
{
    char *end = NULL;
    unsigned long parsed;
    int base = 10;

    if (*text == '#') {
        text++;
    }
    if (*text == '$') {
        text++;
        base = 16;
    } else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
    }
    if (*text == '\0') {
        return false;
    }
    parsed = strtoul(text, &end, base);
    if (end == text || *end != '\0' || parsed > 0xfffful) {
        return false;
    }
    *value = (uint16_t)parsed;
    return true;
}

static void strip_parens(char *text)
{
    size_t length;
    text = trim(text);
    length = strlen(text);
    if (length >= 2u && text[0] == '(' && text[length - 1u] == ')') {
        memmove(text, text + 1, length - 2u);
        text[length - 2u] = '\0';
    }
}

static bool parse_value(Assembler *assembler, const char *text, uint16_t *value)
{
    char temp[128];
    copy_text(temp, sizeof(temp), text);
    strip_parens(temp);
    if (parse_number(trim(temp), value)) {
        return true;
    }
    if (find_constant(temp, value)) {
        return true;
    }
    if (assembler->pass == 2 && find_label(assembler, temp, value)) {
        return true;
    }
    if (assembler->pass == 1) {
        *value = 0u;
        return true;
    }
    return false;
}

static bool parse_byte_value(Assembler *assembler, const char *text, uint8_t *value)
{
    uint16_t parsed;
    if (!parse_value(assembler, text, &parsed) || parsed > 0xffu) {
        return false;
    }
    *value = (uint8_t)parsed;
    return true;
}

static bool parse_target_value(Assembler *assembler, const char *text, uint16_t *value)
{
    char temp[128];
    copy_text(temp, sizeof(temp), text);
    strip_parens(temp);
    if (parse_number(trim(temp), value)) {
        return true;
    }
    if (assembler->pass == 2 && find_label(assembler, temp, value)) {
        return true;
    }
    if (find_constant(temp, value)) {
        return true;
    }
    if (assembler->pass == 1) {
        *value = 0u;
        return true;
    }
    return false;
}

static int register_code(const char *text)
{
    char temp[32];
    symbol_name(temp, sizeof(temp), text);
    if (strcmp(temp, "B") == 0) return 0;
    if (strcmp(temp, "C") == 0) return 1;
    if (strcmp(temp, "D") == 0) return 2;
    if (strcmp(temp, "E") == 0) return 3;
    if (strcmp(temp, "H") == 0) return 4;
    if (strcmp(temp, "L") == 0) return 5;
    if (strcmp(temp, "(HL)") == 0) return 6;
    if (strcmp(temp, "A") == 0) return 7;
    return -1;
}

static int register_pair_code(const char *text)
{
    char temp[32];
    symbol_name(temp, sizeof(temp), text);
    if (strcmp(temp, "BC") == 0) return 0;
    if (strcmp(temp, "DE") == 0) return 1;
    if (strcmp(temp, "HL") == 0) return 2;
    if (strcmp(temp, "SP") == 0) return 3;
    return -1;
}

static bool emit8(Assembler *assembler, uint8_t value)
{
    if (assembler->length >= CAZ_PROGRAM_MAX) {
        copy_text(assembler->image->error, sizeof(assembler->image->error), "assembled program is too large");
        assembler->ok = false;
        return false;
    }
    if (assembler->pass == 2) {
        assembler->image->bytes[assembler->length] = value;
    }
    assembler->length++;
    assembler->address++;
    return true;
}

static bool emit16(Assembler *assembler, uint16_t value)
{
    return emit8(assembler, (uint8_t)(value & 0xffu)) && emit8(assembler, (uint8_t)(value >> 8));
}

static bool split_operands(char *operands, char **left, char **right)
{
    char *comma = strchr(operands, ',');
    if (comma == NULL) {
        return false;
    }
    *comma = '\0';
    *left = trim(operands);
    *right = trim(comma + 1);
    return true;
}

static int jp_condition_opcode(const char *condition)
{
    char temp[32];
    symbol_name(temp, sizeof(temp), condition);
    if (strcmp(temp, "NZ") == 0) return 0xc2;
    if (strcmp(temp, "Z") == 0) return 0xca;
    if (strcmp(temp, "NC") == 0) return 0xd2;
    if (strcmp(temp, "C") == 0) return 0xda;
    if (strcmp(temp, "PO") == 0) return 0xe2;
    if (strcmp(temp, "PE") == 0) return 0xea;
    if (strcmp(temp, "P") == 0) return 0xf2;
    if (strcmp(temp, "M") == 0) return 0xfa;
    return -1;
}

static int jr_condition_opcode(const char *condition)
{
    char temp[32];
    symbol_name(temp, sizeof(temp), condition);
    if (strcmp(temp, "NZ") == 0) return 0x20;
    if (strcmp(temp, "Z") == 0) return 0x28;
    if (strcmp(temp, "NC") == 0) return 0x30;
    if (strcmp(temp, "C") == 0) return 0x38;
    return -1;
}

static bool assemble_ld(Assembler *assembler, char *operands, int line_number)
{
    char *left;
    char *right;
    int left_reg;
    int right_reg;
    int pair;
    uint16_t word;
    uint8_t byte;
    char right_trimmed[128];

    if (!split_operands(operands, &left, &right)) {
        return fail_at(assembler, line_number, "LD needs two operands");
    }

    left_reg = register_code(left);
    right_reg = register_code(right);
    if (left_reg >= 0 && right_reg >= 0) {
        return emit8(assembler, (uint8_t)(0x40u | ((uint8_t)left_reg << 3) | (uint8_t)right_reg));
    }

    if (left_reg >= 0 && parse_byte_value(assembler, right, &byte)) {
        return emit8(assembler, (uint8_t)(0x06u | ((uint8_t)left_reg << 3))) && emit8(assembler, byte);
    }

    pair = register_pair_code(left);
    if (pair >= 0 && parse_value(assembler, right, &word)) {
        return emit8(assembler, (uint8_t)(0x01u | ((uint8_t)pair << 4))) && emit16(assembler, word);
    }

    copy_text(right_trimmed, sizeof(right_trimmed), right);
    if (equals_ci(left, "A") && right_trimmed[0] == '(' && parse_value(assembler, right_trimmed, &word)) {
        return emit8(assembler, 0x3au) && emit16(assembler, word);
    }

    if (left[0] == '(' && equals_ci(right, "A") && parse_value(assembler, left, &word)) {
        return emit8(assembler, 0x32u) && emit16(assembler, word);
    }

    return fail_at(assembler, line_number, "unsupported LD operands");
}

static bool assemble_in(Assembler *assembler, char *operands, int line_number)
{
    char *left;
    char *right;
    uint8_t port;
    if (!split_operands(operands, &left, &right) || !equals_ci(left, "A")) {
        return fail_at(assembler, line_number, "IN syntax is IN A,(PORT)");
    }
    if (!parse_byte_value(assembler, right, &port)) {
        return fail_at(assembler, line_number, "unknown IN port '%s'", right);
    }
    return emit8(assembler, 0xdbu) && emit8(assembler, port);
}

static bool assemble_out(Assembler *assembler, char *operands, int line_number)
{
    char *left;
    char *right;
    uint8_t port;
    if (!split_operands(operands, &left, &right) || !equals_ci(right, "A")) {
        return fail_at(assembler, line_number, "OUT syntax is OUT (PORT),A");
    }
    if (!parse_byte_value(assembler, left, &port)) {
        return fail_at(assembler, line_number, "unknown OUT port '%s'", left);
    }
    return emit8(assembler, 0xd3u) && emit8(assembler, port);
}

static bool assemble_cp(Assembler *assembler, char *operands, int line_number)
{
    int reg = register_code(operands);
    uint8_t byte;
    if (reg >= 0) {
        return emit8(assembler, (uint8_t)(0xb8u | (uint8_t)reg));
    }
    if (!parse_byte_value(assembler, operands, &byte)) {
        return fail_at(assembler, line_number, "unknown CP operand '%s'", operands);
    }
    return emit8(assembler, 0xfeu) && emit8(assembler, byte);
}

static bool assemble_single_operand_alu(Assembler *assembler,
                                        char *operands,
                                        int line_number,
                                        uint8_t reg_base,
                                        uint8_t immediate_opcode,
                                        const char *name)
{
    int reg = register_code(operands);
    uint8_t byte;
    if (reg >= 0) {
        return emit8(assembler, (uint8_t)(reg_base | (uint8_t)reg));
    }
    if (!parse_byte_value(assembler, operands, &byte)) {
        return fail_at(assembler, line_number, "unknown %s operand '%s'", name, operands);
    }
    return emit8(assembler, immediate_opcode) && emit8(assembler, byte);
}

static bool assemble_add(Assembler *assembler, char *operands, int line_number)
{
    char *left;
    char *right;
    if (!split_operands(operands, &left, &right) || !equals_ci(left, "A")) {
        return fail_at(assembler, line_number, "ADD syntax is ADD A,operand");
    }
    return assemble_single_operand_alu(assembler, right, line_number, 0x80u, 0xc6u, "ADD");
}

static bool assemble_jp(Assembler *assembler, char *operands, int line_number)
{
    char *left;
    char *right;
    uint16_t target;
    int opcode = 0xc3;
    if (split_operands(operands, &left, &right)) {
        opcode = jp_condition_opcode(left);
        if (opcode < 0) {
            return fail_at(assembler, line_number, "unknown JP condition '%s'", left);
        }
        operands = right;
    }
    if (!parse_target_value(assembler, operands, &target)) {
        return fail_at(assembler, line_number, "unknown JP target '%s'", operands);
    }
    return emit8(assembler, (uint8_t)opcode) && emit16(assembler, target);
}

static bool assemble_call(Assembler *assembler, char *operands, int line_number)
{
    uint16_t target;
    if (!parse_target_value(assembler, operands, &target)) {
        return fail_at(assembler, line_number, "unknown CALL target '%s'", operands);
    }
    return emit8(assembler, 0xcdu) && emit16(assembler, target);
}

static bool assemble_jr(Assembler *assembler, char *operands, int line_number)
{
    char *left;
    char *right;
    uint16_t target;
    int opcode = 0x18;
    int displacement;
    uint16_t next_address;
    if (split_operands(operands, &left, &right)) {
        opcode = jr_condition_opcode(left);
        if (opcode < 0) {
            return fail_at(assembler, line_number, "unknown JR condition '%s'", left);
        }
        operands = right;
    }
    if (!parse_target_value(assembler, operands, &target)) {
        return fail_at(assembler, line_number, "unknown JR target '%s'", operands);
    }
    next_address = (uint16_t)(assembler->address + 2u);
    displacement = (int)target - (int)next_address;
    if (assembler->pass == 2 && (displacement < -128 || displacement > 127)) {
        return fail_at(assembler, line_number, "JR target out of range '%s'", operands);
    }
    return emit8(assembler, (uint8_t)opcode) && emit8(assembler, (uint8_t)(int8_t)displacement);
}

static bool assemble_inc_dec(Assembler *assembler, char *operands, int line_number, uint8_t base_opcode, const char *name)
{
    int reg = register_code(operands);
    if (reg < 0) {
        return fail_at(assembler, line_number, "%s needs a register operand", name);
    }
    return emit8(assembler, (uint8_t)(base_opcode | ((uint8_t)reg << 3)));
}

static bool assemble_line(Assembler *assembler, char *line, int line_number)
{
    char *comment;
    char *text;
    char *colon;
    char *mnemonic;
    char *operands;

    comment = strchr(line, ';');
    if (comment != NULL) {
        *comment = '\0';
    }
    text = trim(line);
    while ((colon = strchr(text, ':')) != NULL) {
        *colon = '\0';
        if (assembler->pass == 1 && !add_label(assembler, trim(text), line_number)) {
            return false;
        }
        text = trim(colon + 1);
        if (*text == '\0') {
            return true;
        }
    }

    if (*text == '\0') {
        return true;
    }

    mnemonic = text;
    while (*text != '\0' && !isspace((unsigned char)*text)) {
        text++;
    }
    if (*text != '\0') {
        *text = '\0';
        text++;
    }
    operands = trim(text);

    if (equals_ci(mnemonic, "NOP")) return emit8(assembler, 0x00u);
    if (equals_ci(mnemonic, "HALT")) return emit8(assembler, 0x76u);
    if (equals_ci(mnemonic, "RET")) return emit8(assembler, 0xc9u);
    if (equals_ci(mnemonic, "DI")) return emit8(assembler, 0xf3u);
    if (equals_ci(mnemonic, "EI")) return emit8(assembler, 0xfbu);
    if (equals_ci(mnemonic, "LD")) return assemble_ld(assembler, operands, line_number);
    if (equals_ci(mnemonic, "IN")) return assemble_in(assembler, operands, line_number);
    if (equals_ci(mnemonic, "OUT")) return assemble_out(assembler, operands, line_number);
    if (equals_ci(mnemonic, "CP")) return assemble_cp(assembler, operands, line_number);
    if (equals_ci(mnemonic, "JP")) return assemble_jp(assembler, operands, line_number);
    if (equals_ci(mnemonic, "JR")) return assemble_jr(assembler, operands, line_number);
    if (equals_ci(mnemonic, "CALL")) return assemble_call(assembler, operands, line_number);
    if (equals_ci(mnemonic, "INC")) return assemble_inc_dec(assembler, operands, line_number, 0x04u, "INC");
    if (equals_ci(mnemonic, "DEC")) return assemble_inc_dec(assembler, operands, line_number, 0x05u, "DEC");
    if (equals_ci(mnemonic, "ADD")) return assemble_add(assembler, operands, line_number);
    if (equals_ci(mnemonic, "SUB")) return assemble_single_operand_alu(assembler, operands, line_number, 0x90u, 0xd6u, "SUB");
    if (equals_ci(mnemonic, "AND")) return assemble_single_operand_alu(assembler, operands, line_number, 0xa0u, 0xe6u, "AND");
    if (equals_ci(mnemonic, "XOR")) return assemble_single_operand_alu(assembler, operands, line_number, 0xa8u, 0xeeu, "XOR");
    if (equals_ci(mnemonic, "OR")) return assemble_single_operand_alu(assembler, operands, line_number, 0xb0u, 0xf6u, "OR");

    return fail_at(assembler, line_number, "unknown instruction '%s'", mnemonic);
}

static void scan_metadata(CazProgramImage *image, const char *source)
{
    char line[CAZ_MAX_LINE];
    const char *cursor = source;
    while (*cursor != '\0') {
        size_t length = 0u;
        char *text;
        while (cursor[length] != '\0' && cursor[length] != '\n' && length + 1u < sizeof(line)) {
            line[length] = cursor[length];
            length++;
        }
        line[length] = '\0';
        text = trim(line);
        if (*text == ';') {
            text = trim(text + 1);
            if (starts_ci(text, "name:")) {
                copy_text(image->name, sizeof(image->name), trim(text + 5));
            } else if (starts_ci(text, "description:")) {
                copy_text(image->description, sizeof(image->description), trim(text + 12));
            }
        }
        cursor += length;
        if (*cursor == '\n') {
            cursor++;
        }
    }
}

static bool assemble_pass(Assembler *assembler, const char *source)
{
    char line[CAZ_MAX_LINE];
    const char *cursor = source;
    int line_number = 1;
    assembler->address = CAZ_PROGRAM_BASE;
    assembler->length = 0u;

    while (*cursor != '\0') {
        size_t length = 0u;
        while (cursor[length] != '\0' && cursor[length] != '\n' && length + 1u < sizeof(line)) {
            line[length] = cursor[length];
            length++;
        }
        line[length] = '\0';
        if (!assemble_line(assembler, line, line_number)) {
            return false;
        }
        cursor += length;
        if (*cursor == '\n') {
            cursor++;
            line_number++;
        }
    }

    if (assembler->pass == 2) {
        assembler->image->length = assembler->length;
    }
    return true;
}

static void default_metadata_from_source_name(CazProgramImage *image, const char *source_name)
{
    const char *base = source_name != NULL ? source_name : "program";
    const char *slash = strrchr(base, '/');
    const char *dot;
    char name[CAZ_PROGRAM_NAME_MAX];
    if (slash != NULL) {
        base = slash + 1;
    }
    copy_text(name, sizeof(name), base);
    dot = strrchr(name, '.');
    if (dot != NULL) {
        name[dot - name] = '\0';
    }
    copy_text(image->name, sizeof(image->name), name);
    copy_text(image->description, sizeof(image->description), "loaded from .caz source");
}

bool caz_loader_assemble_source(const char *source, const char *source_name, CazProgramImage *image)
{
    Assembler assembler;
    memset(image, 0, sizeof(*image));
    default_metadata_from_source_name(image, source_name);
    copy_text(image->path, sizeof(image->path), source_name != NULL ? source_name : "<memory>");
    scan_metadata(image, source);

    memset(&assembler, 0, sizeof(assembler));
    assembler.image = image;
    assembler.ok = true;
    assembler.pass = 1;
    if (!assemble_pass(&assembler, source)) {
        return false;
    }

    assembler.pass = 2;
    assembler.ok = true;
    if (!assemble_pass(&assembler, source)) {
        return false;
    }

    return true;
}

static bool read_text_file(const char *path, char **out_text, CazProgramImage *image)
{
    FILE *file;
    long size;
    size_t read_count;
    char *buffer;
    file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(image->error, sizeof(image->error), "could not open %s", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        snprintf(image->error, sizeof(image->error), "could not seek %s", path);
        return false;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        snprintf(image->error, sizeof(image->error), "could not size %s", path);
        return false;
    }
    rewind(file);
    buffer = (char *)malloc((size_t)size + 1u);
    if (buffer == NULL) {
        fclose(file);
        snprintf(image->error, sizeof(image->error), "out of memory reading %s", path);
        return false;
    }
    read_count = fread(buffer, 1u, (size_t)size, file);
    fclose(file);
    buffer[read_count] = '\0';
    *out_text = buffer;
    return true;
}

static bool source_buffer_reserve(SourceBuffer *buffer, size_t extra)
{
    size_t required = buffer->length + extra + 1u;
    char *grown;
    if (required <= buffer->capacity) {
        return true;
    }
    size_t capacity = buffer->capacity == 0u ? 1024u : buffer->capacity;
    while (capacity < required) {
        capacity *= 2u;
    }
    grown = (char *)realloc(buffer->data, capacity);
    if (grown == NULL) {
        return false;
    }
    buffer->data = grown;
    buffer->capacity = capacity;
    return true;
}

static bool source_buffer_append(SourceBuffer *buffer, const char *text, size_t length)
{
    if (!source_buffer_reserve(buffer, length)) {
        return false;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool source_buffer_append_cstr(SourceBuffer *buffer, const char *text)
{
    return source_buffer_append(buffer, text, strlen(text));
}

static void directory_name(char *buffer, size_t buffer_length, const char *path)
{
    const char *slash = path != NULL ? strrchr(path, '/') : NULL;
    if (buffer_length == 0u) {
        return;
    }
    if (slash == NULL) {
        copy_text(buffer, buffer_length, ".");
        return;
    }
    if ((size_t)(slash - path) >= buffer_length) {
        buffer[0] = '\0';
        return;
    }
    memcpy(buffer, path, (size_t)(slash - path));
    buffer[slash - path] = '\0';
}

static bool parse_include_directive(char *line, char *include_name, size_t include_name_length)
{
    char *text = trim(line);
    char *start;
    char *end;
    const char *directive = NULL;

    if (starts_ci(text, "INCLUDE")) {
        directive = "INCLUDE";
    } else if (starts_ci(text, ".INCLUDE")) {
        directive = ".INCLUDE";
    } else {
        return false;
    }

    text = trim(text + strlen(directive));
    if (*text != '"') {
        return false;
    }
    start = text + 1;
    end = strchr(start, '"');
    if (end == NULL || end == start || end[1] != '\0') {
        return false;
    }
    if ((size_t)(end - start) >= include_name_length) {
        return false;
    }
    memcpy(include_name, start, (size_t)(end - start));
    include_name[end - start] = '\0';
    return true;
}

static bool resolve_include_path(char *buffer,
                                 size_t buffer_length,
                                 const char *source_path,
                                 const char *include_name)
{
    char directory[CAZ_PROGRAM_PATH_MAX];
    int written;
    if (include_name[0] == '/') {
        written = snprintf(buffer, buffer_length, "%s", include_name);
    } else {
        directory_name(directory, sizeof(directory), source_path);
        written = snprintf(buffer, buffer_length, "%s/%s", directory, include_name);
    }
    return written > 0 && (size_t)written < buffer_length;
}

static bool expand_includes_into(SourceBuffer *out,
                                 const char *source,
                                 const char *source_path,
                                 unsigned depth,
                                 CazProgramImage *image)
{
    const char *cursor = source;
    int line_number = 1;
    if (depth > 8u) {
        snprintf(image->error, sizeof(image->error), "%s:%d: include nesting is too deep", source_path, line_number);
        return false;
    }

    while (*cursor != '\0') {
        char line[CAZ_MAX_LINE];
        char line_copy[CAZ_MAX_LINE];
        char include_name[CAZ_PROGRAM_PATH_MAX];
        size_t length = 0u;
        while (cursor[length] != '\0' && cursor[length] != '\n' && length + 1u < sizeof(line)) {
            line[length] = cursor[length];
            length++;
        }
        line[length] = '\0';
        copy_text(line_copy, sizeof(line_copy), line);

        if (parse_include_directive(line_copy, include_name, sizeof(include_name))) {
            char include_path[CAZ_PROGRAM_PATH_MAX];
            char *include_source = NULL;
            if (!resolve_include_path(include_path, sizeof(include_path), source_path, include_name)) {
                snprintf(image->error, sizeof(image->error), "%s:%d: include path is too long: %s", source_path, line_number, include_name);
                return false;
            }
            if (!read_text_file(include_path, &include_source, image)) {
                char read_error[CAZ_LOADER_ERROR_MAX];
                copy_text(read_error, sizeof(read_error), image->error);
                snprintf(image->error, sizeof(image->error), "%s:%d: could not include %s: %s", source_path, line_number, include_name, read_error);
                return false;
            }
            if (!source_buffer_append_cstr(out, "\n") ||
                !expand_includes_into(out, include_source, include_path, depth + 1u, image) ||
                !source_buffer_append_cstr(out, "\n")) {
                free(include_source);
                if (image->error[0] == '\0') {
                    copy_text(image->error, sizeof(image->error), "out of memory expanding includes");
                }
                return false;
            }
            free(include_source);
        } else {
            if (!source_buffer_append(out, cursor, length) ||
                !source_buffer_append_cstr(out, "\n")) {
                copy_text(image->error, sizeof(image->error), "out of memory expanding includes");
                return false;
            }
        }

        cursor += length;
        if (*cursor == '\n') {
            cursor++;
        }
        line_number++;
    }
    return true;
}

bool caz_loader_load_file(CazCpu *cpu, const char *path, CazProgramImage *image)
{
    char *source = NULL;
    SourceBuffer expanded = {0};
    bool ok;
    memset(image, 0, sizeof(*image));
    copy_text(image->path, sizeof(image->path), path);
    if (!read_text_file(path, &source, image)) {
        return false;
    }
    ok = expand_includes_into(&expanded, source, path, 0u, image);
    if (ok) {
        ok = caz_loader_assemble_source(expanded.data, path, image);
    }
    free(expanded.data);
    free(source);
    if (!ok) {
        return false;
    }
    if (!caz_cpu_load(cpu, CAZ_PROGRAM_BASE, image->bytes, image->length)) {
        copy_text(image->error, sizeof(image->error), "assembled image does not fit in Caz memory");
        return false;
    }
    return true;
}

static const CazProgramMetadata program_registry[CAZ_PROGRAM_COUNT] = {
    {CAZ_PROGRAM_CURIOUS_PATROL,
     "curious-patrol",
     "general house-cat patrol loop that tracks motion, listens at night, and retreats from loud shocks",
     1u, 1u, 0u, 2.2f, 1u},
    {CAZ_PROGRAM_STALK_AND_POUNCE,
     "stalk-and-pounce",
     "hedgerow hunting sketch that crouches, stalks, pounces, and recovers from startled footing",
     1u, 1u, 0u, 2.6f, 3u},
    {CAZ_PROGRAM_FARMYARD_CAUTION,
     "farmyard-caution",
     "rural safety routine for machine avoidance, terrain caution, fatigue, and recovery",
     1u, 1u, 0u, 1.1f, 4u},
    {CAZ_PROGRAM_FERAL_FORAGER,
     "feral-forager",
     "standalone feral-cat routine that favors sunning, wire tapping, hiding, and cautious foraging",
     1u, 1u, 1u, 1.25f, 2u},
};

size_t caz_loader_program_count(void)
{
    return sizeof(program_registry) / sizeof(program_registry[0]);
}

const CazProgramMetadata *caz_loader_program_metadata(CazProgramKind kind)
{
    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        if (program_registry[index].kind == kind) {
            return &program_registry[index];
        }
    }
    return NULL;
}

const CazProgramMetadata *caz_loader_program_metadata_at(size_t index)
{
    if (index >= caz_loader_program_count()) {
        return NULL;
    }
    return &program_registry[index];
}

bool caz_loader_program_path(CazProgramKind kind, const char *program_dir, char *buffer, size_t buffer_length)
{
    const CazProgramMetadata *metadata = caz_loader_program_metadata(kind);
    const char *dir = (program_dir != NULL && program_dir[0] != '\0') ? program_dir : "programs";
    int written;
    if (metadata == NULL || buffer == NULL || buffer_length == 0u) {
        return false;
    }
    written = snprintf(buffer, buffer_length, "%s/%s.caz", dir, metadata->name);
    return written > 0 && (size_t)written < buffer_length;
}

bool caz_loader_load_named(CazCpu *cpu,
                           CazProgramKind kind,
                           const char *program_dir,
                           CazProgramImage *image)
{
    char path[CAZ_PROGRAM_PATH_MAX];
    if (!caz_loader_program_path(kind, program_dir, path, sizeof(path))) {
        memset(image, 0, sizeof(*image));
        copy_text(image->error, sizeof(image->error), "program path is too long or unknown");
        return false;
    }
    return caz_loader_load_file(cpu, path, image);
}

bool caz_loader_parse_program_name(const char *name, CazProgramKind *kind)
{
    if (name == NULL || kind == NULL) {
        return false;
    }
    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        if (equals_ci(name, program_registry[index].name)) {
            *kind = program_registry[index].kind;
            return true;
        }
    }
    if (equals_ci(name, "patrol")) {
        *kind = CAZ_PROGRAM_CURIOUS_PATROL;
    } else if (equals_ci(name, "caution")) {
        *kind = CAZ_PROGRAM_FARMYARD_CAUTION;
    } else if (equals_ci(name, "pounce")) {
        *kind = CAZ_PROGRAM_STALK_AND_POUNCE;
    } else if (equals_ci(name, "feral") || equals_ci(name, "forager")) {
        *kind = CAZ_PROGRAM_FERAL_FORAGER;
    } else {
        return false;
    }
    return true;
}

const char *caz_loader_program_name(CazProgramKind kind)
{
    const CazProgramMetadata *metadata = caz_loader_program_metadata(kind);
    return metadata != NULL ? metadata->name : "unknown";
}

const char *caz_loader_program_description(CazProgramKind kind)
{
    const CazProgramMetadata *metadata = caz_loader_program_metadata(kind);
    return metadata != NULL ? metadata->description : "unknown";
}

void caz_loader_print_programs(FILE *out)
{
    for (size_t index = 0u; index < caz_loader_program_count(); index++) {
        const CazProgramMetadata *metadata = &program_registry[index];
        fprintf(out,
                "%-17s programs/%s.caz - %s [%s%s%s]\n",
                metadata->name,
                metadata->name,
                metadata->description,
                metadata->survival_participant ? "survival" : "demo",
                metadata->standalone_energy ? ", standalone-energy" : "",
                metadata->cazenv_assignable ? ", cazenv" : ", path-only");
    }
}
