#include "pd_mgba.h"

#include <string.h>

#include <mgba/core/core.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/gba/gba.h>

// Where the main loop waits for VBlank: the System stack's depth seen most at
// a frame's end (the app's pk_rest_note / pk_rest_sp).
#define REST_SLOTS 8
static uint32_t restSp[REST_SLOTS];
static uint32_t restCount[REST_SLOTS];

static uint32_t sys_sp(struct ARMCore* cpu) {
    return cpu->privilegeMode == MODE_SYSTEM || cpu->privilegeMode == MODE_USER
        ? (uint32_t) cpu->gprs[ARM_SP] : (uint32_t) cpu->bankedRegisters[BANK_NONE][0];
}

void pd_mgba_reset(void) {
    memset(restSp, 0, sizeof(restSp));
    memset(restCount, 0, sizeof(restCount));
}

void pd_mgba_frame_done(struct mCore* core) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    if (cpu->privilegeMode == MODE_IRQ) return;
    uint32_t sp = sys_sp(cpu);
    int low = 0;
    for (int i = 0; i < REST_SLOTS; i++) {
        if (restSp[i] == sp) {
            if (restCount[i] < 100000) restCount[i]++;
            return;
        }
        if (restCount[i] < restCount[low]) low = i;
    }
    restSp[low] = sp;
    restCount[low] = 1;
}

static uint32_t rest_sp(void) {
    int best = 0;
    for (int i = 1; i < REST_SLOTS; i++) {
        if (restCount[i] > restCount[best]) best = i;
    }
    return restCount[best] >= 30 ? restSp[best] : 0;
}

// Whether the game's code can be called now: waiting for VBlank with nothing of
// its own in progress - not in an interrupt, its stack back at the main loop's
// depth. A call mid-way through the game's own read-modify-writes broke it
// (the app's 2026-10-20 FLY black screen).
static bool call_safe(struct mCore* core) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    if (cpu->privilegeMode == MODE_IRQ) return false;
    uint32_t rest = rest_sp();
    return !rest || sys_sp(cpu) == rest;
}

// Calls Thumb function fn with r0-r3 = args and returns once it has, leaving
// the CPU exactly as it was: run instruction by instruction with IRQs masked
// until it returns to SENTINEL (in the ROM header, never executed), then put
// everything back - registers, pipeline, the fetch region, idle detection.
#define SENTINEL 0x080000C0u
static bool call(void* ctx, uint32_t fn, const uint32_t args[4], uint32_t* r0) {
    struct mCore* core = ctx;
    if (!call_safe(core)) return false;
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    struct ARMRegisterFile saved = cpu->regs;
    uint32_t prefetch0 = cpu->prefetch[0], prefetch1 = cpu->prefetch[1];
    struct ARMMemory mem = cpu->memory;
    struct GBA* gba = (struct GBA*) core->board;
    int region = gba->memory.activeRegion;
    uint32_t biosPrefetch = gba->memory.biosPrefetch;
    uint32_t idleLoop = gba->idleLoop, lastJump = gba->lastJump;
    bool haltPending = gba->haltPending;
    int idleStep = gba->idleDetectionStep, idleFailures = gba->idleDetectionFailures;
    int32_t cached[16];
    memcpy(cached, gba->cachedRegisters, sizeof(cached));
    enum ExecutionMode exec = cpu->executionMode;
    int halted = cpu->halted;
    enum PrivilegeMode priv = cpu->privilegeMode;
    cpu->halted = 0;
    cpu->cpsr.i = 1;
    if (priv != MODE_SYSTEM && priv != MODE_USER) {
        ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
        cpu->cpsr.priv = MODE_SYSTEM;
    }
    for (int i = 0; i < 4; i++) cpu->gprs[i] = (int32_t) args[i];
    cpu->gprs[ARM_LR] = (int32_t) (SENTINEL | 1);
    cpu->gprs[ARM_PC] = (int32_t) (fn & ~1u);
    _ARMSetMode(cpu, MODE_THUMB);
    ThumbWritePC(cpu);
    bool ok = false;
    for (int i = 0; i < 2000000; i++) {
        if (cpu->executionMode == MODE_THUMB && (uint32_t) cpu->gprs[ARM_PC] - WORD_SIZE_THUMB == SENTINEL) {
            ok = true;
            break;
        }
        core->step(core);
    }
    if (r0) *r0 = (uint32_t) cpu->gprs[0];
    cpu->regs = saved;
    cpu->privilegeMode = priv;
    cpu->executionMode = exec == MODE_ARM ? MODE_THUMB : MODE_ARM; // so _ARMSetMode applies
    _ARMSetMode(cpu, exec);
    cpu->prefetch[0] = prefetch0;
    cpu->prefetch[1] = prefetch1;
    cpu->memory = mem;
    gba->memory.activeRegion = region;
    gba->memory.biosPrefetch = biosPrefetch;
    gba->idleLoop = idleLoop;
    gba->lastJump = lastJump;
    gba->haltPending = haltPending;
    gba->idleDetectionStep = idleStep;
    gba->idleDetectionFailures = idleFailures;
    memcpy(gba->cachedRegisters, cached, sizeof(cached));
    cpu->halted = halted;
    return ok;
}

// EWRAM / IWRAM straight from the core's memory; anything else (the ROM) on the bus.
static uint8_t* ram_at(struct mCore* core, uint32_t addr, size_t len) {
    size_t size;
    uint32_t base = addr & 0xFF000000u;
    if (base != 0x02000000u && base != 0x03000000u) return NULL;
    uint8_t* block = core->getMemoryBlock(core, base >> 24, &size);
    uint32_t off = addr - base;
    return block && off + len <= size ? block + off : NULL;
}

static bool mem_read(void* ctx, uint32_t addr, void* out, size_t len) {
    struct mCore* core = ctx;
    uint8_t* ram = ram_at(core, addr, len);
    if (ram) {
        memcpy(out, ram, len);
        return true;
    }
    for (size_t i = 0; i < len; i++) ((uint8_t*) out)[i] = (uint8_t) core->rawRead8(core, addr + (uint32_t) i, -1);
    return true;
}

static bool mem_write(void* ctx, uint32_t addr, const void* in, size_t len) {
    uint8_t* ram = ram_at(ctx, addr, len);
    if (!ram) return false;
    memcpy(ram, in, len);
    return true;
}

void pd_mgba_io(struct pd_game_io* io, struct mCore* core) {
    io->ctx = core;
    io->read = mem_read;
    io->write = mem_write;
    io->call = call;
}
