#ifndef QUICK_TEST_API_H
#define QUICK_TEST_API_H
#include <stdint.h>
#include <stdio.h>
#define CHERRYPI
typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;typedef int32_t s32;
typedef FILE *file_tag_type;
#include "../source/gpsp/input.h"
typedef enum { SDL_AUDIO_STOPPED,SDL_AUDIO_PLAYING,SDL_AUDIO_PAUSED } SDL_audiostatus;
extern u32 savestate_slot;
extern int cherrypi_menu_active,screen_scale;
SDL_audiostatus SDL_GetAudioStatus(void);
void SDL_PauseAudio(int);
void print_string(const char*,u16,u16,u32,u32);
void blit_to_screen(u16*,u32,u32,u32,u32);
void clear_screen(u16);
void cherrypi_menu_present(void);
void video_resolution_large(void);
void video_resolution_small(void);
void set_gba_resolution(int);
void get_savestate_filename_noshot(u32,char*);
u32 menu_advanced(u16*);
int load_state(char*);
int save_state(char*,u16*);
int save_config_file(void);
void save_controllers(void);
void quit_without_autosave(void);
#endif
