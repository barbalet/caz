#include "caz_programs.h"

#include "caz_droid.h"

#include <string.h>

typedef struct Builder {
    CazProgramImage *image;
    bool ok;
} Builder;

static void emit8(Builder *builder, uint8_t value)
{
    if (!builder->ok || builder->image->length >= CAZ_PROGRAM_MAX) {
        builder->ok = false;
        return;
    }
    builder->image->bytes[builder->image->length++] = value;
}

static void emit16(Builder *builder, uint16_t value)
{
    emit8(builder, (uint8_t)(value & 0xffu));
    emit8(builder, (uint8_t)(value >> 8));
}

static size_t mark(const Builder *builder)
{
    return builder->image->length;
}

static void patch16(Builder *builder, size_t offset, uint16_t value)
{
    if (!builder->ok || offset + 1u >= builder->image->length) {
        builder->ok = false;
        return;
    }
    builder->image->bytes[offset] = (uint8_t)(value & 0xffu);
    builder->image->bytes[offset + 1u] = (uint8_t)(value >> 8);
}

static uint16_t absolute(size_t offset)
{
    return (uint16_t)(CAZ_PROGRAM_BASE + offset);
}

static size_t emit_jp_placeholder(Builder *builder, uint8_t opcode)
{
    size_t patch_at;
    emit8(builder, opcode);
    patch_at = mark(builder);
    emit16(builder, 0u);
    return patch_at;
}

static void emit_jp(Builder *builder, size_t target)
{
    emit8(builder, 0xc3u);
    emit16(builder, absolute(target));
}

static void emit_in_a(Builder *builder, uint8_t port)
{
    emit8(builder, 0xdbu);
    emit8(builder, port);
}

static void emit_out_a(Builder *builder, uint8_t port)
{
    emit8(builder, 0xd3u);
    emit8(builder, port);
}

static void emit_cp(Builder *builder, uint8_t value)
{
    emit8(builder, 0xfeu);
    emit8(builder, value);
}

static void emit_ld_a(Builder *builder, uint8_t value)
{
    emit8(builder, 0x3eu);
    emit8(builder, value);
}

static void emit_set_port(Builder *builder, uint8_t port, uint8_t value)
{
    emit_ld_a(builder, value);
    emit_out_a(builder, port);
}

static void emit_pose(Builder *builder,
                      uint8_t gait,
                      uint8_t head,
                      uint8_t ears,
                      uint8_t tail,
                      uint8_t vocal,
                      uint8_t eyelid)
{
    emit_set_port(builder, CAZ_PORT_GAIT, gait);
    emit_set_port(builder, CAZ_PORT_HEAD_YAW, head);
    emit_set_port(builder, CAZ_PORT_EAR_POSE, ears);
    emit_set_port(builder, CAZ_PORT_TAIL_POSE, tail);
    emit_set_port(builder, CAZ_PORT_VOCAL, vocal);
    emit_set_port(builder, CAZ_PORT_EYELID, eyelid);
}

static void build_curious_patrol(Builder *builder)
{
    size_t to_alarm;
    size_t to_track;
    size_t to_mouse;
    size_t to_night;
    size_t to_close;
    size_t loop = mark(builder);

    emit_in_a(builder, CAZ_PORT_EAR_VOLUME);
    emit_cp(builder, 190u);
    to_alarm = emit_jp_placeholder(builder, 0xd2u);

    emit_in_a(builder, CAZ_PORT_EYE_MOTION);
    emit_cp(builder, 150u);
    to_track = emit_jp_placeholder(builder, 0xd2u);

    emit_in_a(builder, CAZ_PORT_EAR_PATTERN);
    emit_cp(builder, 1u);
    to_mouse = emit_jp_placeholder(builder, 0xcau);

    emit_in_a(builder, CAZ_PORT_EYE_LUMA);
    emit_cp(builder, 32u);
    to_night = emit_jp_placeholder(builder, 0xdau);

    emit_in_a(builder, CAZ_PORT_EYE_EDGE);
    emit_cp(builder, 210u);
    to_close = emit_jp_placeholder(builder, 0xd2u);

    emit_pose(builder, 1u, 128u, 1u, 5u, 0u, 150u);
    emit_jp(builder, loop);

    patch16(builder, to_track, absolute(mark(builder)));
    emit_pose(builder, 1u, 196u, 2u, 7u, 2u, 210u);
    emit_jp(builder, loop);

    patch16(builder, to_mouse, absolute(mark(builder)));
    emit_pose(builder, 2u, 96u, 2u, 3u, 0u, 224u);
    emit_jp(builder, loop);

    patch16(builder, to_alarm, absolute(mark(builder)));
    emit_pose(builder, 4u, 64u, 3u, 8u, 4u, 240u);
    emit_jp(builder, loop);

    patch16(builder, to_night, absolute(mark(builder)));
    emit_pose(builder, 0u, 128u, 4u, 2u, 3u, 255u);
    emit_jp(builder, loop);

    patch16(builder, to_close, absolute(mark(builder)));
    emit_pose(builder, 5u, 144u, 2u, 4u, 1u, 200u);
    emit_jp(builder, loop);
}

static void build_nap_watch(Builder *builder)
{
    size_t to_startle;
    size_t to_open_eye;
    size_t to_greet;
    size_t loop = mark(builder);

    emit_in_a(builder, CAZ_PORT_EAR_VOLUME);
    emit_cp(builder, 210u);
    to_startle = emit_jp_placeholder(builder, 0xd2u);

    emit_in_a(builder, CAZ_PORT_EYE_MOTION);
    emit_cp(builder, 185u);
    to_open_eye = emit_jp_placeholder(builder, 0xd2u);

    emit_in_a(builder, CAZ_PORT_EAR_PATTERN);
    emit_cp(builder, 2u);
    to_greet = emit_jp_placeholder(builder, 0xcau);

    emit_pose(builder, 0u, 128u, 0u, 2u, 3u, 58u);
    emit_jp(builder, loop);

    patch16(builder, to_open_eye, absolute(mark(builder)));
    emit_pose(builder, 0u, 160u, 4u, 4u, 1u, 180u);
    emit_jp(builder, loop);

    patch16(builder, to_startle, absolute(mark(builder)));
    emit_pose(builder, 4u, 92u, 3u, 8u, 4u, 248u);
    emit_jp(builder, loop);

    patch16(builder, to_greet, absolute(mark(builder)));
    emit_pose(builder, 1u, 128u, 1u, 6u, 5u, 164u);
    emit_jp(builder, loop);
}

static void build_farmyard_mouser(Builder *builder)
{
    size_t to_machine;
    size_t to_prey;
    size_t to_human;
    size_t to_weather;
    size_t to_glare;
    size_t loop = mark(builder);

    emit_in_a(builder, CAZ_PORT_EAR_PATTERN);
    emit_cp(builder, 4u);
    to_machine = emit_jp_placeholder(builder, 0xcau);

    emit_in_a(builder, CAZ_PORT_EAR_PATTERN);
    emit_cp(builder, 1u);
    to_prey = emit_jp_placeholder(builder, 0xcau);

    emit_in_a(builder, CAZ_PORT_EAR_PATTERN);
    emit_cp(builder, 2u);
    to_human = emit_jp_placeholder(builder, 0xcau);

    emit_in_a(builder, CAZ_PORT_EAR_PATTERN);
    emit_cp(builder, 3u);
    to_weather = emit_jp_placeholder(builder, 0xcau);

    emit_in_a(builder, CAZ_PORT_EYE_LUMA);
    emit_cp(builder, 222u);
    to_glare = emit_jp_placeholder(builder, 0xd2u);

    emit_pose(builder, 1u, 132u, 1u, 5u, 0u, 176u);
    emit_jp(builder, loop);

    patch16(builder, to_machine, absolute(mark(builder)));
    emit_pose(builder, 4u, 80u, 3u, 8u, 4u, 235u);
    emit_jp(builder, loop);

    patch16(builder, to_prey, absolute(mark(builder)));
    emit_in_a(builder, CAZ_PORT_EYE_EDGE);
    emit_cp(builder, 168u);
    {
        size_t to_pounce = emit_jp_placeholder(builder, 0xd2u);
        emit_pose(builder, 2u, 108u, 2u, 3u, 0u, 225u);
        emit_jp(builder, loop);
        patch16(builder, to_pounce, absolute(mark(builder)));
        emit_pose(builder, 3u, 168u, 2u, 7u, 2u, 252u);
        emit_jp(builder, loop);
    }

    patch16(builder, to_human, absolute(mark(builder)));
    emit_pose(builder, 1u, 128u, 1u, 6u, 3u, 160u);
    emit_jp(builder, loop);

    patch16(builder, to_weather, absolute(mark(builder)));
    emit_pose(builder, 0u, 128u, 4u, 1u, 0u, 118u);
    emit_jp(builder, loop);

    patch16(builder, to_glare, absolute(mark(builder)));
    emit_pose(builder, 1u, 96u, 1u, 5u, 0u, 90u);
    emit_jp(builder, loop);
}

bool caz_program_build(CazProgramKind kind, CazProgramImage *image)
{
    Builder builder;
    memset(image, 0, sizeof(*image));
    image->name = caz_program_name(kind);
    image->description = caz_program_description(kind);
    builder.image = image;
    builder.ok = true;

    switch (kind) {
    case CAZ_PROGRAM_CURIOUS_PATROL:
        build_curious_patrol(&builder);
        break;
    case CAZ_PROGRAM_NAP_WATCH:
        build_nap_watch(&builder);
        break;
    case CAZ_PROGRAM_FARMYARD_MOUSER:
        build_farmyard_mouser(&builder);
        break;
    default:
        builder.ok = false;
        break;
    }

    emit8(&builder, 0x76u);
    return builder.ok;
}

bool caz_program_load(CazCpu *cpu, CazProgramKind kind, CazProgramImage *image)
{
    if (!caz_program_build(kind, image)) {
        return false;
    }
    return caz_cpu_load(cpu, CAZ_PROGRAM_BASE, image->bytes, image->length);
}

bool caz_program_parse(const char *name, CazProgramKind *kind)
{
    if (strcmp(name, "curious-patrol") == 0 || strcmp(name, "patrol") == 0) {
        *kind = CAZ_PROGRAM_CURIOUS_PATROL;
    } else if (strcmp(name, "nap-watch") == 0 || strcmp(name, "nap") == 0) {
        *kind = CAZ_PROGRAM_NAP_WATCH;
    } else if (strcmp(name, "farmyard-mouser") == 0 || strcmp(name, "mouser") == 0) {
        *kind = CAZ_PROGRAM_FARMYARD_MOUSER;
    } else {
        return false;
    }
    return true;
}

const char *caz_program_name(CazProgramKind kind)
{
    switch (kind) {
    case CAZ_PROGRAM_CURIOUS_PATROL: return "curious-patrol";
    case CAZ_PROGRAM_NAP_WATCH: return "nap-watch";
    case CAZ_PROGRAM_FARMYARD_MOUSER: return "farmyard-mouser";
    default: return "unknown";
    }
}

const char *caz_program_description(CazProgramKind kind)
{
    switch (kind) {
    case CAZ_PROGRAM_CURIOUS_PATROL:
        return "general house-cat patrol loop that tracks motion, listens at night, and retreats from loud shocks";
    case CAZ_PROGRAM_NAP_WATCH:
        return "low-energy parlour mode that dozes until motion, human speech, or a startling sound appears";
    case CAZ_PROGRAM_FARMYARD_MOUSER:
        return "rural mouser routine tuned for prey rustle, tractor noise, human calls, weather, and glare";
    default:
        return "unknown";
    }
}

void caz_program_print_all(FILE *out)
{
    fprintf(out, "curious-patrol - %s\n", caz_program_description(CAZ_PROGRAM_CURIOUS_PATROL));
    fprintf(out, "nap-watch      - %s\n", caz_program_description(CAZ_PROGRAM_NAP_WATCH));
    fprintf(out, "farmyard-mouser - %s\n", caz_program_description(CAZ_PROGRAM_FARMYARD_MOUSER));
}
