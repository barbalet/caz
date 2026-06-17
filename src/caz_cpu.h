#ifndef CAZ_CPU_H
#define CAZ_CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CAZ_MEMORY_SIZE 65536u
#define CAZ_RESET_VECTOR 0x0100u
#define CAZ_DEFAULT_STACK 0xfffeu

#define CAZ_FLAG_S  0x80u
#define CAZ_FLAG_Z  0x40u
#define CAZ_FLAG_H  0x10u
#define CAZ_FLAG_PV 0x04u
#define CAZ_FLAG_N  0x02u
#define CAZ_FLAG_C  0x01u

typedef uint8_t (*CazIoRead)(void *user, uint8_t port);
typedef void (*CazIoWrite)(void *user, uint8_t port, uint8_t value);

typedef struct CazCpu {
    uint8_t a;
    uint8_t f;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t pc;
    uint16_t sp;
    uint8_t memory[CAZ_MEMORY_SIZE];
    bool halted;
    bool trace;
    uint64_t cycles;
    uint64_t instructions;
    CazIoRead io_read;
    CazIoWrite io_write;
    void *io_user;
} CazCpu;

void caz_cpu_init(CazCpu *cpu, CazIoRead read_cb, CazIoWrite write_cb, void *user);
void caz_cpu_reset(CazCpu *cpu);
bool caz_cpu_load(CazCpu *cpu, uint16_t address, const uint8_t *program, size_t length);
int caz_cpu_step(CazCpu *cpu);
uint64_t caz_cpu_run(CazCpu *cpu, uint64_t max_instructions);
void caz_cpu_dump(const CazCpu *cpu, FILE *out);

uint16_t caz_cpu_hl(const CazCpu *cpu);
void caz_cpu_set_hl(CazCpu *cpu, uint16_t value);

#endif
