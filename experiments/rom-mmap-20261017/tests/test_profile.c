#define CP_PROFILE_TEST 1
#include "../source/gpsp/cherrypi/profile.c"
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
static void *check_mask(void *unused)
{
    sigset_t mask;
    (void)unused;
    assert(!pthread_sigmask(SIG_SETMASK, NULL, &mask));
    assert(sigismember(&mask, SIGALRM));
    return NULL;
}
int main(int argc, char **argv)
{
    unsigned i, sum = 0;
    pthread_t thread;
    (void)argv;
    if (argc > 1) {
        setenv("CP_PROFILE", "1", 1);
        cp_profile_prepare();
        assert(!pthread_create(&thread, NULL, check_mask, NULL));
        assert(!pthread_join(thread, NULL));
        cp_profile_start();
        for (i = 0; i < 30; ++i) {
            struct timespec remaining = {0, 10000000};
            while (nanosleep(&remaining, &remaining)) {}
        }
        assert(total >= 15 && total < 100);
        return 0;
    }
    cp_profile_prepare(); cp_profile_start();
    assert(!enabled && !started);
    cp_smc_write_trace[3]=16;
    cp_profile_origin(0x100,0x200,100);
    assert(cp_smc_write_trace[3]==0 && origins[0].count==0);
    cp_profile_translation(0x03000100, 0, 0); cp_profile_flush(0); cp_profile_frame();
    assert(translations[0] == 0 && flushes[0] == 0 && game_frames == 0);
    enabled = started = 1;
    cp_smc_write_trace[0]=0x03001234; cp_smc_write_trace[1]=2;
    cp_smc_write_trace[2]=1; cp_smc_write_trace[3]=16;
    cp_profile_origin(0x1234,0x08005678,4096);
    assert(origins[0].count==1 && origins[0].address==0x03001234 && origins[0].width==16);
    assert(origins[0].old_value==1 && origins[0].new_value==2 && cp_smc_write_trace[3]==0);
    cp_profile_origin(0x1234,0x08005678,2048);
    assert(origins[1].count==1 && origins[1].width==0);
    cp_profile_translation(0x03000100, 0, 0);
    cp_profile_translation(0x03000100, 0, 0);
    cp_profile_translation(0x03000100, 0, 1);
    cp_profile_translation(0x08000100, 1, 1);
    cp_profile_translation(0x00000100, 2, 0);
    cp_profile_translation(0, 3, 0); cp_profile_translation(0, 0, 2);
    cp_profile_flush(0); cp_profile_flush(1); cp_profile_flush(2); cp_profile_flush(3);
    cp_profile_frame();
    assert(translations[0]==2 && translations[1]==1 && translations[3]==1 && translations[4]==1);
    assert(flushes[0]==1 && flushes[1]==1 && flushes[2]==1 && game_frames==1);
    rom_begin=0x1000; rom_end=0x2000;
    ram_begin=0x3000; ram_end=0x4000;
    bios_begin=0x5000; bios_end=0x6000;
    sample_pc(0x1000); sample_pc(0x1fff);
    sample_pc(0x3000); sample_pc(0x5000);
    assert(rom_hits==2 && ram_hits==1 && bios_hits==1);
    for (i=0; i<MAX_SAMPLES+10; ++i) sample_pc(0x10000 + i*4);
    for (i=0; i<BUCKETS; ++i) sum+=samples[i].count;
    assert(total==MAX_SAMPLES && sum+lost+rom_hits+ram_hits+bios_hits==(unsigned)total);
    assert(lost>0);
    assert(timeline_size==120 && timeline[0].frames==1 && timeline[0].translations[0]==2);
    assert(timeline[119].flushes[2]==1);
    puts("PASS profiler: disabled by default, range boundaries, collisions, cap and sample totals");
    return 0;
}
