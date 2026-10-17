/* Checked, staged gpSP savestates. GPL-2.0-or-later. */
#ifndef CHERRYPI_STATE_IO_H
#define CHERRYPI_STATE_IO_H
#include <stddef.h>
#include <stdint.h>
#define CP_STATE_PAYLOAD 506951u
#define CP_STATE_LEGACY 506947u
#define CP_STATE_HEADER 64u
#define CP_STATE_PREVIEW (240u * 160u * 2u)
int cp_state_begin(size_t prefix);
void cp_state_pack(void *field, size_t size, const char *name);
unsigned char *cp_state_snapshot(void);
int cp_state_layout_ok(void);
int cp_state_set_rom(const char *path);
int cp_state_stage(const char *path, const char *live_name);
const void *cp_state_field(const void *field, size_t size);
void cp_state_commit(void);
int cp_state_save(const char *path);
int cp_state_preview(const char *path, void *pixels, void *timestamp, size_t time_size);
const char *cp_state_message(void);
void cp_state_clear_message(void);
int cp_state_fail(const char *text);
int cp_state_legacy(void);
#endif
