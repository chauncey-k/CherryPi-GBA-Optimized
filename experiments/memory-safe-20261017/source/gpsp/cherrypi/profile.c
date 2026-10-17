/* GPL-2.0-or-later. No allocation, logging or clock reads in the handler.
 * SIGALRM is blocked before SDL creates threads, then enabled only on main.
 * Samples describe main-thread wall time, including waits, not CPU percentages.
 */
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include "profile.h"
unsigned cp_live_smc_skipped;
#ifndef CP_PROFILE_TEST
#include "common.h"
#endif
#define BUCKETS 4096
#define MAX_SAMPLES 12000
static struct { uintptr_t pc; unsigned count; } samples[BUCKETS];
static volatile sig_atomic_t total, lost, rom_hits, ram_hits, bios_hits;
static uintptr_t rom_begin, rom_end, ram_begin, ram_end, bios_begin, bios_end;
static int enabled, started;
/* All writes to these counters originate on the emulation thread. */
static volatile unsigned translations[6], flushes[3], game_frames;
static struct { unsigned pc, thumb, count; } guest_blocks[512];
static unsigned guest_lost;
static struct { unsigned frames, translations[6], flushes[3]; } timeline[120];
static unsigned timeline_size;
/* Written by guarded ARM stores only; consumed by the next RAM flush. */
unsigned cp_smc_write_trace[4]; /* host address, new value, old value, width bits */
static struct {
    unsigned caller, pc, address, width, old_value, new_value, used, count;
} origins[64];
static unsigned origin_lost;

void cp_profile_origin(unsigned caller, unsigned pc, unsigned cache_used)
{
    unsigned width=cp_smc_write_trace[3], address=cp_smc_write_trace[0], i;
    cp_smc_write_trace[3]=0;
    if (!enabled || !started) return;
#ifndef CP_PROFILE_TEST
    if (width) {
        uintptr_t host=address, ir=(uintptr_t)iwram, er=(uintptr_t)ewram;
        if (host>=ir+0x8000 && host<ir+0x10000)
            address=0x03000000u+(unsigned)(host-ir-0x8000);
        else if (host>=er && host<er+0x80000 && ((host-er)&0xffff)>=0x8000)
            address=0x02000000u+(unsigned)((host-er)>>16)*0x8000u+((host-er)&0x7fff);
        else address=0xffffffffu;
    } else address=0;
#endif
    for (i=0;i<64;i++) {
        if (!origins[i].count || (origins[i].caller==caller && origins[i].pc==pc && origins[i].width==width)) {
            origins[i].caller=caller; origins[i].pc=pc; origins[i].width=width;
            origins[i].address=address; origins[i].old_value=cp_smc_write_trace[2];
            origins[i].new_value=cp_smc_write_trace[1];
            origins[i].used=cache_used>origins[i].used?cache_used:origins[i].used;
            ++origins[i].count;
            return;
        }
    }
    ++origin_lost;
}

void cp_profile_translation(unsigned pc, unsigned region, unsigned thumb)
{
    unsigned slot, i;
    if (!enabled || !started || region > 2 || thumb > 1) return;
    ++translations[region * 2 + thumb];
    slot = (((pc >> 1) ^ thumb) * 2654435761u) & 511;
    for (i = 0; i < 16; ++i, slot = (slot + 1) & 511) {
        if (!guest_blocks[slot].count ||
            (guest_blocks[slot].pc == pc && guest_blocks[slot].thumb == thumb)) {
            guest_blocks[slot].pc = pc;
            guest_blocks[slot].thumb = thumb;
            ++guest_blocks[slot].count;
            return;
        }
    }
    ++guest_lost;
}

void cp_profile_flush(unsigned region)
{
    if (enabled && started && region < 3) ++flushes[region];
}

void cp_profile_frame(void)
{
    if (enabled && started) ++game_frames;
}

static void sample_pc(uintptr_t pc)
{
    unsigned slot, n;
    if (total >= MAX_SAMPLES) return;
    ++total;
    if (total % 100 == 0 && timeline_size < 120) {
        unsigned i;
        timeline[timeline_size].frames = game_frames;
        for (i = 0; i < 6; ++i) timeline[timeline_size].translations[i] = translations[i];
        for (i = 0; i < 3; ++i) timeline[timeline_size].flushes[i] = flushes[i];
        ++timeline_size;
    }
    if (pc >= rom_begin && pc < rom_end) { ++rom_hits; return; }
    if (pc >= ram_begin && pc < ram_end) { ++ram_hits; return; }
    if (pc >= bios_begin && pc < bios_end) { ++bios_hits; return; }
    pc &= ~(uintptr_t)3;
    slot = ((pc >> 2) * 2654435761u) & (BUCKETS - 1);
    for (n = 0; n < 16; ++n, slot = (slot + 1) & (BUCKETS - 1)) {
        if (!samples[slot].count || samples[slot].pc == pc) {
            samples[slot].pc = pc;
            ++samples[slot].count;
            return;
        }
    }
    ++lost;
}

static void on_tick(int sig, siginfo_t *info, void *context)
{
    (void)sig; (void)info;
#if defined(__arm__)
    sample_pc(((ucontext_t *)context)->uc_mcontext.arm_pc);
#elif defined(CP_PROFILE_TEST) && defined(__x86_64__)
    sample_pc(((ucontext_t *)context)->uc_mcontext.gregs[REG_RIP]);
#else
#error Unsupported profiling context
#endif
}

static void dump_profile(void)
{
    struct itimerval off = {0};
    sigset_t mask;
    FILE *out, *maps;
    unsigned i;
    char line[512];
    if (!started) return;
    sigemptyset(&mask); sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    setitimer(ITIMER_REAL, &off, NULL);
    started = 0;
    out = fopen("/tmp/cherrypi-profile.txt", "w");
    if (!out) { perror("Profile log"); return; }
    fprintf(out, "CP_PROFILE_V2 interval_ms=10 limit=%d main_thread_wall_time=1\n", MAX_SAMPLES);
    fprintf(out, "TOTAL %d\nLOST %d\nJIT_ROM %d\nJIT_RAM %d\nJIT_BIOS %d\n",
            total, lost, rom_hits, ram_hits, bios_hits);
    for (i = 0; i < BUCKETS; ++i)
        if (samples[i].count)
            fprintf(out, "PC %lx %u\n", (unsigned long)samples[i].pc, samples[i].count);
    fprintf(out, "GAME_FRAMES %u\nGUEST_UNTRACKED %u\n", game_frames, guest_lost);
    for (i = 0; i < 6; ++i) fprintf(out, "TRANSLATIONS %u %u\n", i, translations[i]);
    for (i = 0; i < 3; ++i) fprintf(out, "FLUSHES %u %u\n", i, flushes[i]);
    for (i = 0; i < 512; ++i)
        if (guest_blocks[i].count)
            fprintf(out, "GUEST %08x %u %u\n", guest_blocks[i].pc, guest_blocks[i].thumb, guest_blocks[i].count);
    for (i = 0; i < timeline_size; ++i) {
        unsigned j;
        fprintf(out, "TRACE %u %u", i + 1, timeline[i].frames);
        for (j = 0; j < 6; ++j) fprintf(out, " %u", timeline[i].translations[j]);
        for (j = 0; j < 3; ++j) fprintf(out, " %u", timeline[i].flushes[j]);
        fputc('\n', out);
    }
    fprintf(out,"LIVE_SMC_RETAINED %u\n",cp_live_smc_skipped);
    fprintf(out,"ORIGIN_LOST %u\n",origin_lost);
    for (i=0;i<64;i++) if (origins[i].count)
        fprintf(out,"ORIGIN %08x %08x %08x %u %08x %08x %u %u\n",
                origins[i].caller,origins[i].pc,origins[i].address,origins[i].width,
                origins[i].old_value,origins[i].new_value,origins[i].used,origins[i].count);
    fputs("MAPS\n", out);
    maps = fopen("/proc/self/maps", "r");
    if (maps) { while (fgets(line, sizeof(line), maps)) fputs(line, out); fclose(maps); }
    if (fclose(out)) perror("Profile close");
    else fprintf(stderr, "Profile saved: /tmp/cherrypi-profile.txt (%d samples, %d lost)\n", total, lost);
}

void cp_profile_prepare(void)
{
    sigset_t mask;
    const char *value = getenv("CP_PROFILE");
    enabled = value && !strcmp(value, "1");
    if (!enabled) return;
    sigemptyset(&mask); sigaddset(&mask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL)) { perror("Profile signal mask"); enabled = 0; }
}

void cp_profile_start(void)
{
    struct sigaction action;
    struct itimerval timer = {{0, 10000}, {0, 10000}};
    sigset_t mask;
    if (!enabled || started) return;
#ifndef CP_PROFILE_TEST
    rom_begin = (uintptr_t)rom_translation_cache;
    rom_end = rom_begin + sizeof(rom_translation_cache);
    ram_begin = (uintptr_t)ram_translation_cache;
    ram_end = ram_begin + sizeof(ram_translation_cache);
    bios_begin = (uintptr_t)bios_translation_cache;
    bios_end = bios_begin + sizeof(bios_translation_cache);
#endif
    memset(&action, 0, sizeof(action));
    action.sa_sigaction = on_tick;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGALRM, &action, NULL)) { perror("Profile sigaction"); return; }
    if (atexit(dump_profile)) { fputs("Profile atexit failed\n", stderr); return; }
    started = 1;
    if (setitimer(ITIMER_REAL, &timer, NULL)) { perror("Profile timer"); started = 0; return; }
    sigemptyset(&mask); sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    fputs("Profile active: main-thread wall-time samples every 10 ms; exit normally to write log\n", stderr);
}
