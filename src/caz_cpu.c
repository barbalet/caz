#include "caz_cpu.h"

#include <string.h>

static uint8_t parity_even(uint8_t value)
{
    value ^= (uint8_t)(value >> 4);
    value &= 0x0fu;
    return (uint8_t)((0x6996u >> value) & 1u) == 0u;
}

uint16_t caz_cpu_hl(const CazCpu *cpu)
{
    return (uint16_t)(((uint16_t)cpu->h << 8) | cpu->l);
}

void caz_cpu_set_hl(CazCpu *cpu, uint16_t value)
{
    cpu->h = (uint8_t)(value >> 8);
    cpu->l = (uint8_t)(value & 0xffu);
}

static uint16_t bc(const CazCpu *cpu)
{
    return (uint16_t)(((uint16_t)cpu->b << 8) | cpu->c);
}

static uint16_t de(const CazCpu *cpu)
{
    return (uint16_t)(((uint16_t)cpu->d << 8) | cpu->e);
}

static void set_bc(CazCpu *cpu, uint16_t value)
{
    cpu->b = (uint8_t)(value >> 8);
    cpu->c = (uint8_t)value;
}

static void set_de(CazCpu *cpu, uint16_t value)
{
    cpu->d = (uint8_t)(value >> 8);
    cpu->e = (uint8_t)value;
}

static uint8_t fetch8(CazCpu *cpu)
{
    return cpu->memory[cpu->pc++];
}

static uint16_t fetch16(CazCpu *cpu)
{
    uint8_t lo = fetch8(cpu);
    uint8_t hi = fetch8(cpu);
    return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static void push16(CazCpu *cpu, uint16_t value)
{
    cpu->sp = (uint16_t)(cpu->sp - 1u);
    cpu->memory[cpu->sp] = (uint8_t)(value >> 8);
    cpu->sp = (uint16_t)(cpu->sp - 1u);
    cpu->memory[cpu->sp] = (uint8_t)value;
}

static uint16_t pop16(CazCpu *cpu)
{
    uint8_t lo = cpu->memory[cpu->sp++];
    uint8_t hi = cpu->memory[cpu->sp++];
    return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static void set_szp(CazCpu *cpu, uint8_t value, uint8_t preserved)
{
    uint8_t flags = preserved;
    if ((value & 0x80u) != 0u) {
        flags |= CAZ_FLAG_S;
    }
    if (value == 0u) {
        flags |= CAZ_FLAG_Z;
    }
    if (parity_even(value)) {
        flags |= CAZ_FLAG_PV;
    }
    cpu->f = flags;
}

static void set_add_flags(CazCpu *cpu, uint8_t lhs, uint8_t rhs, uint16_t sum)
{
    uint8_t result = (uint8_t)sum;
    uint8_t flags = 0u;
    if ((result & 0x80u) != 0u) {
        flags |= CAZ_FLAG_S;
    }
    if (result == 0u) {
        flags |= CAZ_FLAG_Z;
    }
    if (((lhs & 0x0fu) + (rhs & 0x0fu)) > 0x0fu) {
        flags |= CAZ_FLAG_H;
    }
    if ((((lhs ^ result) & (rhs ^ result)) & 0x80u) != 0u) {
        flags |= CAZ_FLAG_PV;
    }
    if (sum > 0xffu) {
        flags |= CAZ_FLAG_C;
    }
    cpu->f = flags;
}

static void set_sub_flags(CazCpu *cpu, uint8_t lhs, uint8_t rhs, uint16_t diff)
{
    uint8_t result = (uint8_t)diff;
    uint8_t flags = CAZ_FLAG_N;
    if ((result & 0x80u) != 0u) {
        flags |= CAZ_FLAG_S;
    }
    if (result == 0u) {
        flags |= CAZ_FLAG_Z;
    }
    if ((lhs & 0x0fu) < (rhs & 0x0fu)) {
        flags |= CAZ_FLAG_H;
    }
    if ((((lhs ^ rhs) & (lhs ^ result)) & 0x80u) != 0u) {
        flags |= CAZ_FLAG_PV;
    }
    if (diff > 0xffu) {
        flags |= CAZ_FLAG_C;
    }
    cpu->f = flags;
}

static uint8_t read_reg(CazCpu *cpu, uint8_t code)
{
    switch (code & 7u) {
    case 0: return cpu->b;
    case 1: return cpu->c;
    case 2: return cpu->d;
    case 3: return cpu->e;
    case 4: return cpu->h;
    case 5: return cpu->l;
    case 6: return cpu->memory[caz_cpu_hl(cpu)];
    default: return cpu->a;
    }
}

static void write_reg(CazCpu *cpu, uint8_t code, uint8_t value)
{
    switch (code & 7u) {
    case 0: cpu->b = value; break;
    case 1: cpu->c = value; break;
    case 2: cpu->d = value; break;
    case 3: cpu->e = value; break;
    case 4: cpu->h = value; break;
    case 5: cpu->l = value; break;
    case 6: cpu->memory[caz_cpu_hl(cpu)] = value; break;
    default: cpu->a = value; break;
    }
}

static void add_a(CazCpu *cpu, uint8_t value)
{
    uint8_t lhs = cpu->a;
    uint16_t sum = (uint16_t)lhs + value;
    cpu->a = (uint8_t)sum;
    set_add_flags(cpu, lhs, value, sum);
}

static void sub_a(CazCpu *cpu, uint8_t value)
{
    uint8_t lhs = cpu->a;
    uint16_t diff = (uint16_t)lhs - value;
    cpu->a = (uint8_t)diff;
    set_sub_flags(cpu, lhs, value, diff);
}

static void cp_a(CazCpu *cpu, uint8_t value)
{
    set_sub_flags(cpu, cpu->a, value, (uint16_t)cpu->a - value);
}

static void logic_result(CazCpu *cpu, uint8_t value, uint8_t extra_flags)
{
    set_szp(cpu, value, extra_flags);
    cpu->a = value;
}

static void inc_reg(CazCpu *cpu, uint8_t code)
{
    uint8_t old_value = read_reg(cpu, code);
    uint8_t value = (uint8_t)(old_value + 1u);
    uint8_t carry = (uint8_t)(cpu->f & CAZ_FLAG_C);
    write_reg(cpu, code, value);
    set_szp(cpu, value, carry);
    if ((old_value & 0x0fu) == 0x0fu) {
        cpu->f |= CAZ_FLAG_H;
    }
    if (old_value == 0x7fu) {
        cpu->f |= CAZ_FLAG_PV;
    }
}

static void dec_reg(CazCpu *cpu, uint8_t code)
{
    uint8_t old_value = read_reg(cpu, code);
    uint8_t value = (uint8_t)(old_value - 1u);
    uint8_t carry = (uint8_t)(cpu->f & CAZ_FLAG_C);
    write_reg(cpu, code, value);
    set_szp(cpu, value, (uint8_t)(carry | CAZ_FLAG_N));
    if ((old_value & 0x0fu) == 0u) {
        cpu->f |= CAZ_FLAG_H;
    }
    if (old_value == 0x80u) {
        cpu->f |= CAZ_FLAG_PV;
    }
}

static bool condition_is_true(const CazCpu *cpu, uint8_t opcode)
{
    switch (opcode) {
    case 0xc2: return (cpu->f & CAZ_FLAG_Z) == 0u;
    case 0xca: return (cpu->f & CAZ_FLAG_Z) != 0u;
    case 0xd2: return (cpu->f & CAZ_FLAG_C) == 0u;
    case 0xda: return (cpu->f & CAZ_FLAG_C) != 0u;
    case 0xe2: return (cpu->f & CAZ_FLAG_PV) == 0u;
    case 0xea: return (cpu->f & CAZ_FLAG_PV) != 0u;
    case 0xf2: return (cpu->f & CAZ_FLAG_S) == 0u;
    case 0xfa: return (cpu->f & CAZ_FLAG_S) != 0u;
    default: return true;
    }
}

void caz_cpu_init(CazCpu *cpu, CazIoRead read_cb, CazIoWrite write_cb, void *user)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->io_read = read_cb;
    cpu->io_write = write_cb;
    cpu->io_user = user;
    caz_cpu_reset(cpu);
}

void caz_cpu_reset(CazCpu *cpu)
{
    cpu->a = 0u;
    cpu->f = 0u;
    cpu->b = 0u;
    cpu->c = 0u;
    cpu->d = 0u;
    cpu->e = 0u;
    cpu->h = 0u;
    cpu->l = 0u;
    cpu->pc = CAZ_RESET_VECTOR;
    cpu->sp = CAZ_DEFAULT_STACK;
    cpu->halted = false;
    cpu->cycles = 0u;
    cpu->instructions = 0u;
}

bool caz_cpu_load(CazCpu *cpu, uint16_t address, const uint8_t *program, size_t length)
{
    if (length > (size_t)CAZ_MEMORY_SIZE || (size_t)address + length > (size_t)CAZ_MEMORY_SIZE) {
        return false;
    }
    memcpy(&cpu->memory[address], program, length);
    return true;
}

int caz_cpu_step(CazCpu *cpu)
{
    if (cpu->halted) {
        return 0;
    }

    uint16_t old_pc = cpu->pc;
    uint8_t opcode = fetch8(cpu);
    int cycles = 4;

    if (cpu->trace) {
        fprintf(stderr,
                "pc=%04x op=%02x af=%02x%02x bc=%04x de=%04x hl=%04x sp=%04x\n",
                old_pc,
                opcode,
                cpu->a,
                cpu->f,
                bc(cpu),
                de(cpu),
                caz_cpu_hl(cpu),
                cpu->sp);
    }

    if (opcode == 0x76u) {
        cpu->halted = true;
        cycles = 4;
    } else if ((opcode & 0xc7u) == 0x06u) {
        write_reg(cpu, (uint8_t)((opcode >> 3) & 7u), fetch8(cpu));
        cycles = ((opcode & 0x38u) == 0x30u) ? 10 : 7;
    } else if ((opcode & 0xc7u) == 0x04u) {
        inc_reg(cpu, (uint8_t)((opcode >> 3) & 7u));
        cycles = ((opcode & 0x38u) == 0x30u) ? 11 : 4;
    } else if ((opcode & 0xc7u) == 0x05u) {
        dec_reg(cpu, (uint8_t)((opcode >> 3) & 7u));
        cycles = ((opcode & 0x38u) == 0x30u) ? 11 : 4;
    } else if (opcode >= 0x40u && opcode <= 0x7fu) {
        uint8_t dst = (uint8_t)((opcode >> 3) & 7u);
        uint8_t src = (uint8_t)(opcode & 7u);
        write_reg(cpu, dst, read_reg(cpu, src));
        cycles = (dst == 6u || src == 6u) ? 7 : 4;
    } else if (opcode >= 0x80u && opcode <= 0x87u) {
        add_a(cpu, read_reg(cpu, (uint8_t)(opcode & 7u)));
        cycles = ((opcode & 7u) == 6u) ? 7 : 4;
    } else if (opcode >= 0x90u && opcode <= 0x97u) {
        sub_a(cpu, read_reg(cpu, (uint8_t)(opcode & 7u)));
        cycles = ((opcode & 7u) == 6u) ? 7 : 4;
    } else if (opcode >= 0xa0u && opcode <= 0xa7u) {
        logic_result(cpu, (uint8_t)(cpu->a & read_reg(cpu, (uint8_t)(opcode & 7u))), CAZ_FLAG_H);
        cycles = ((opcode & 7u) == 6u) ? 7 : 4;
    } else if (opcode >= 0xa8u && opcode <= 0xafu) {
        logic_result(cpu, (uint8_t)(cpu->a ^ read_reg(cpu, (uint8_t)(opcode & 7u))), 0u);
        cycles = ((opcode & 7u) == 6u) ? 7 : 4;
    } else if (opcode >= 0xb0u && opcode <= 0xb7u) {
        logic_result(cpu, (uint8_t)(cpu->a | read_reg(cpu, (uint8_t)(opcode & 7u))), 0u);
        cycles = ((opcode & 7u) == 6u) ? 7 : 4;
    } else if (opcode >= 0xb8u && opcode <= 0xbfu) {
        cp_a(cpu, read_reg(cpu, (uint8_t)(opcode & 7u)));
        cycles = ((opcode & 7u) == 6u) ? 7 : 4;
    } else {
        switch (opcode) {
        case 0x00:
        case 0xf3:
        case 0xfb:
            cycles = 4;
            break;
        case 0x01:
            set_bc(cpu, fetch16(cpu));
            cycles = 10;
            break;
        case 0x11:
            set_de(cpu, fetch16(cpu));
            cycles = 10;
            break;
        case 0x21:
            caz_cpu_set_hl(cpu, fetch16(cpu));
            cycles = 10;
            break;
        case 0x31:
            cpu->sp = fetch16(cpu);
            cycles = 10;
            break;
        case 0x3a: {
            uint16_t address = fetch16(cpu);
            cpu->a = cpu->memory[address];
            cycles = 13;
            break;
        }
        case 0x32: {
            uint16_t address = fetch16(cpu);
            cpu->memory[address] = cpu->a;
            cycles = 13;
            break;
        }
        case 0xc3:
            cpu->pc = fetch16(cpu);
            cycles = 10;
            break;
        case 0xc2:
        case 0xca:
        case 0xd2:
        case 0xda:
        case 0xe2:
        case 0xea:
        case 0xf2:
        case 0xfa: {
            uint16_t address = fetch16(cpu);
            if (condition_is_true(cpu, opcode)) {
                cpu->pc = address;
            }
            cycles = 10;
            break;
        }
        case 0x18: {
            int8_t offset = (int8_t)fetch8(cpu);
            cpu->pc = (uint16_t)(cpu->pc + offset);
            cycles = 12;
            break;
        }
        case 0x20:
        case 0x28:
        case 0x30:
        case 0x38: {
            int8_t offset = (int8_t)fetch8(cpu);
            bool take = false;
            if (opcode == 0x20u) {
                take = (cpu->f & CAZ_FLAG_Z) == 0u;
            } else if (opcode == 0x28u) {
                take = (cpu->f & CAZ_FLAG_Z) != 0u;
            } else if (opcode == 0x30u) {
                take = (cpu->f & CAZ_FLAG_C) == 0u;
            } else {
                take = (cpu->f & CAZ_FLAG_C) != 0u;
            }
            if (take) {
                cpu->pc = (uint16_t)(cpu->pc + offset);
            }
            cycles = take ? 12 : 7;
            break;
        }
        case 0xcd: {
            uint16_t address = fetch16(cpu);
            push16(cpu, cpu->pc);
            cpu->pc = address;
            cycles = 17;
            break;
        }
        case 0xc9:
            cpu->pc = pop16(cpu);
            cycles = 10;
            break;
        case 0xc6:
            add_a(cpu, fetch8(cpu));
            cycles = 7;
            break;
        case 0xd6:
            sub_a(cpu, fetch8(cpu));
            cycles = 7;
            break;
        case 0xe6:
            logic_result(cpu, (uint8_t)(cpu->a & fetch8(cpu)), CAZ_FLAG_H);
            cycles = 7;
            break;
        case 0xee:
            logic_result(cpu, (uint8_t)(cpu->a ^ fetch8(cpu)), 0u);
            cycles = 7;
            break;
        case 0xf6:
            logic_result(cpu, (uint8_t)(cpu->a | fetch8(cpu)), 0u);
            cycles = 7;
            break;
        case 0xfe:
            cp_a(cpu, fetch8(cpu));
            cycles = 7;
            break;
        case 0xdb: {
            uint8_t port = fetch8(cpu);
            cpu->a = cpu->io_read ? cpu->io_read(cpu->io_user, port) : 0xffu;
            cycles = 11;
            break;
        }
        case 0xd3: {
            uint8_t port = fetch8(cpu);
            if (cpu->io_write) {
                cpu->io_write(cpu->io_user, port, cpu->a);
            }
            cycles = 11;
            break;
        }
        default:
            fprintf(stderr, "Caz CPU fault: unsupported opcode 0x%02x at 0x%04x\n", opcode, old_pc);
            cpu->halted = true;
            return -1;
        }
    }

    cpu->cycles += (uint64_t)cycles;
    cpu->instructions++;
    return cycles;
}

uint64_t caz_cpu_run(CazCpu *cpu, uint64_t max_instructions)
{
    uint64_t executed = 0u;
    while (!cpu->halted && executed < max_instructions) {
        if (caz_cpu_step(cpu) < 0) {
            break;
        }
        executed++;
    }
    return executed;
}

void caz_cpu_dump(const CazCpu *cpu, FILE *out)
{
    fprintf(out,
            "AF=%02x%02x BC=%04x DE=%04x HL=%04x PC=%04x SP=%04x cycles=%llu instructions=%llu halted=%s\n",
            cpu->a,
            cpu->f,
            bc(cpu),
            de(cpu),
            caz_cpu_hl(cpu),
            cpu->pc,
            cpu->sp,
            (unsigned long long)cpu->cycles,
            (unsigned long long)cpu->instructions,
            cpu->halted ? "yes" : "no");
}
